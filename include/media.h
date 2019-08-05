#ifndef _MEDIA_H_
#define	_MEDIA_H_
#include "config.h"
#include "log.h"
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <memory>
#include "Buffer.h"

class MediaFrame
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual void onMediaFrame(const MediaFrame &frame) = 0;
		virtual void onMediaFrame(DWORD ssrc, const MediaFrame &frame) = 0;
	};

	class RtpPacketization
	{
	public:
		RtpPacketization(DWORD pos,DWORD size,const BYTE* prefix,DWORD prefixLen)
		{
			//Store values
			this->pos = pos;
			this->size = size;
			this->prefixLen = prefixLen;
			//Check size
			if (prefixLen)
			{
				//Allocate mem
				this->prefix = (BYTE*) malloc(prefixLen);
				//Copy data
				memcpy(this->prefix,prefix,prefixLen);
			} else {
				this->prefix = NULL;
			}

		}
		~RtpPacketization()
		{
			if (prefix) free(prefix);
		}

		DWORD GetPos()		const { return pos;		}
		DWORD GetSize()		const { return size;		}
		BYTE* GetPrefixData()	const { return prefix;		}
		DWORD GetPrefixLen()	const { return prefixLen;	}
		DWORD GetTotalLength()	const { return size+prefixLen;	}
		
		void Dump() const 
		{
			if (!prefixLen)
			{
				Debug("[RtpPacketization size=%u pos=%u/]\n",size,pos);
			} else {
				Debug("[RtpPacketization size=%u pos=%u]\n",size,pos);
				::Dump(prefix,prefixLen);
				Debug("[RtpPacketization/]\n");
			}
			
		}
	private:
		DWORD	pos;
		DWORD	size;
		BYTE*	prefix;
		DWORD	prefixLen;
	};

	typedef std::vector<RtpPacketization*> RtpPacketizationInfo;
public:
	enum Type {Audio=0,Video=1,Text=2,Unknown=-1};

	static const char * TypeToString(Type type)
	{
		switch(type)
		{
			case Audio:
				return "Audio";
			case Video:
				return "Video";
			case Text:
				return "Text";
			default:
				return "Unknown";
		}
		return "Unknown";
	}

	MediaFrame(Type type,DWORD size)
	{
		//Set media type
		this->type = type;
		//Create new owned buffer
		buffer = std::make_shared<Buffer>(size);
		//Owned buffer
		ownedBuffer = true;
	}
	
	MediaFrame(Type type,const std::shared_ptr<Buffer>& buffer)
	{
		//Set media type
		this->type = type;
		//Share same buffer
		this->buffer = buffer;
		//Not owned buffer
		ownedBuffer = false;
	}

	virtual ~MediaFrame()
	{
		//Clear
		ClearRTPPacketizationInfo();
		//Clear memory
		if (configData) free(configData);
	}

	void	ClearRTPPacketizationInfo()
	{
		//Emtpy
		while (!rtpInfo.empty())
		{
			//Delete
			delete(rtpInfo.back());
			//remove
			rtpInfo.pop_back();
		}
	}
	
	void	DumpRTPPacketizationInfo() const
	{
		//Dump all info
		for (const auto& info : rtpInfo)
			info->Dump();
	}
	
	void	AddRtpPacket(DWORD pos,DWORD size,const BYTE* prefix,DWORD prefixLen)		
	{
		rtpInfo.push_back(new RtpPacketization(pos,size,prefix,prefixLen));
	}
	
	Type	GetType() const		{ return type;	}
	DWORD	GetTimeStamp() const	{ return ts;	}
	void	SetTimestamp(DWORD ts)	{ this->ts = ts; }
	
	DWORD	GetSSRC() const		{ return ssrc;		}
	void	SetSSRC(DWORD ssrc)	{ this->ssrc = ssrc;	}

	bool	HasRtpPacketizationInfo() const				{ return !rtpInfo.empty();	}
	const RtpPacketizationInfo& GetRtpPacketizationInfo() const	{ return rtpInfo;		}
	virtual MediaFrame* Clone() const = 0;

	DWORD GetDuration() const		{ return duration;		}
	void SetDuration(DWORD duration)	{ this->duration = duration;	}

	DWORD GetLength() const			{ return buffer->GetSize();			}
	DWORD GetMaxMediaLength() const		{ return buffer->GetCapacity();			}
	const BYTE* GetData() const		{ return buffer->GetData();			}

	BYTE* GetData()				{ AdquireBuffer(); return buffer->GetData();	}
	void SetLength(DWORD length)		{ AdquireBuffer(); buffer->SetSize(length);	}
	
	void ResetData(DWORD size = 0) 
	{
		//Create new owned buffer
		buffer = std::make_shared<Buffer>(size);
		//Owned buffer
		ownedBuffer = true;
	}

	void Alloc(DWORD size)
	{
		//Adquire buffer
		AdquireBuffer();
		//Allocate mem
		buffer->Alloc(size);
	}

	void SetMedia(const BYTE* data,DWORD size)
	{
		//Adquire buffer
		AdquireBuffer();
		//Allocate mem
		buffer->SetData(data,size);
	}

	DWORD AppendMedia(const BYTE* data,DWORD size)
	{
                //Get current pos
                DWORD pos = buffer->GetSize();
		//Adquire buffer
		AdquireBuffer();
		//Append data
		buffer->AppendData(data,size);
		//Return previous pos
		return pos;
	}
        
	BYTE* AllocateCodecConfig(DWORD size)
        {
                //If we had old data
		if (configData)
			//Free it
			free(configData);
		//Allocate memory
		configData = (BYTE*) malloc(size);
                //Set lenght
		configSize = size;
                //return it
                return configData;
        }
        
	void SetCodecConfig(const BYTE* data,DWORD size)
	{
		//Copy
		memcpy(AllocateCodecConfig(size),data,size);
	}
	
	void ClearCodecConfig()
	{
		//Free mem
		if (configData) free(configData);
		//Creal
		configData = nullptr;
		configSize = 0;
	}
	
	void Reset() 
	{
		//Clear packetization info
		ClearRTPPacketizationInfo();
		//Reset data
		ResetData();
		//Clear time
		SetTimestamp((DWORD)-1);
	}
	
	bool HasCodecConfig() const		{ return configData && configSize;	}
	BYTE* GetCodecConfigData() const	{ return configData;			}
	DWORD GetCodecConfigSize() const	{ return configSize;			} 
	
	DWORD GetClockRate() const		{ return clockRate;			}
	void  SetClockRate(DWORD clockRate)	{ this->clockRate = clockRate;		}
	
protected:
	void AdquireBuffer()
	{
		//If already owning
		if (ownedBuffer)
			//Do nothing
			return;
		
		//Clone payload
		buffer = std::make_shared<Buffer>(buffer->GetData(),buffer->GetSize());
		//We own the payload
		ownedBuffer = true;
	}
	
protected:
	Type type		= MediaFrame::Unknown;
	DWORD ts		= (DWORD)-1;
	DWORD ssrc		= 0;
	
	std::shared_ptr<Buffer> buffer;
	bool ownedBuffer	= false;
	
	DWORD	duration	= 0;
	DWORD	clockRate	= 1000;
	
	BYTE	*configData	= nullptr;
	DWORD	configSize	= 0;
	
	RtpPacketizationInfo rtpInfo;
};

#endif	/* MEDIA_H */

