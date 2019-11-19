// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/sink_input_pin_win.h"

#include <cstring>

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/win/win_util.h"
#include "media/base/timestamp_constants.h"

namespace media {

const REFERENCE_TIME kSecondsToReferenceTime = 10000000;

static DWORD GetArea(const BITMAPINFOHEADER& info_header) {
  return info_header.biWidth * info_header.biHeight;
}

SinkInputPin::SinkInputPin(IBaseFilter* filter, SinkFilterObserver* observer)
    : PinBase(filter),
      requested_frame_rate_(0),
      flip_y_(false),
      observer_(observer) {}

void SinkInputPin::SetRequestedMediaFormat(
    VideoPixelFormat pixel_format,
    float frame_rate,
    const BITMAPINFOHEADER& info_header) {
  requested_pixel_format_ = pixel_format;
  requested_frame_rate_ = frame_rate;
  requested_info_header_ = info_header;
  resulting_format_.frame_size.SetSize(0, 0);
  resulting_format_.frame_rate = 0;
  resulting_format_.pixel_format = PIXEL_FORMAT_UNKNOWN;
}

bool SinkInputPin::IsMediaTypeValid(const AM_MEDIA_TYPE* media_type) {
  const GUID type = media_type->majortype;
  if (type != MEDIATYPE_Video)
    return false;

  const GUID format_type = media_type->formattype;
  if (format_type != FORMAT_VideoInfo)
    return false;

  // Check for the sub types we support.
  const GUID sub_type = media_type->subtype;
  VIDEOINFOHEADER* pvi =
      reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);
  if (pvi == NULL)
    return false;

  // Store the incoming width and height.
  resulting_format_.frame_size.SetSize(pvi->bmiHeader.biWidth,
                                       abs(pvi->bmiHeader.biHeight));
  if (pvi->AvgTimePerFrame > 0) {
    resulting_format_.frame_rate =
        static_cast<int>(kSecondsToReferenceTime / pvi->AvgTimePerFrame);
  } else {
    resulting_format_.frame_rate = requested_frame_rate_;
  }
  if (sub_type == kMediaSubTypeI420 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('I', '4', '2', '0')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_I420;
    return true;
  }
  if (sub_type == MEDIASUBTYPE_YUY2 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('Y', 'U', 'Y', '2')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_YUY2;
    return true;
  }
  if (sub_type == MEDIASUBTYPE_MJPG &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('M', 'J', 'P', 'G')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_MJPEG;
    return true;
  }
  if (sub_type == MEDIASUBTYPE_RGB24 &&
      pvi->bmiHeader.biCompression == BI_RGB) {
    resulting_format_.pixel_format = PIXEL_FORMAT_RGB24;
    return true;
  }
  if (sub_type == MEDIASUBTYPE_RGB32 &&
      pvi->bmiHeader.biCompression == BI_RGB) {
    resulting_format_.pixel_format = PIXEL_FORMAT_ARGB;
    flip_y_ = true;
    return true;
  }
  if (sub_type == kMediaSubTypeY16 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('Y', '1', '6', ' ')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_Y16;
    return true;
  }
  if (sub_type == kMediaSubTypeZ16 &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('Z', '1', '6', ' ')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_Y16;
    return true;
  }
  if (sub_type == kMediaSubTypeINVZ &&
      pvi->bmiHeader.biCompression == MAKEFOURCC('I', 'N', 'V', 'Z')) {
    resulting_format_.pixel_format = PIXEL_FORMAT_Y16;
    return true;
  }

  DVLOG(2) << __func__ << " unsupported media type: "
           << base::win::String16FromGUID(sub_type);
  return false;
}

bool SinkInputPin::GetValidMediaType(int index, AM_MEDIA_TYPE* media_type) {
  if (media_type->cbFormat < sizeof(VIDEOINFOHEADER))
    return false;

  VIDEOINFOHEADER* const pvi =
      reinterpret_cast<VIDEOINFOHEADER*>(media_type->pbFormat);

  ZeroMemory(pvi, sizeof(VIDEOINFOHEADER));
  pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  pvi->bmiHeader.biPlanes = 1;
  pvi->bmiHeader.biClrImportant = 0;
  pvi->bmiHeader.biClrUsed = 0;
  if (requested_frame_rate_ > 0)
    pvi->AvgTimePerFrame = kSecondsToReferenceTime / requested_frame_rate_;

  media_type->majortype = MEDIATYPE_Video;
  media_type->formattype = FORMAT_VideoInfo;
  media_type->bTemporalCompression = FALSE;

  if (requested_pixel_format_ == PIXEL_FORMAT_MJPEG ||
      requested_pixel_format_ == PIXEL_FORMAT_Y16) {
    // If the requested pixel format is MJPEG or Y16, don't accept other.
    // This is ok since the capabilities of the capturer have been
    // enumerated and we know that it is supported.
    if (index != 0)
      return false;

    pvi->bmiHeader = requested_info_header_;
    return true;
  }

  switch (index) {
    case 0: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('I', '4', '2', '0');
      pvi->bmiHeader.biBitCount = 12;  // bit per pixel
      pvi->bmiHeader.biWidth = requested_info_header_.biWidth;
      pvi->bmiHeader.biHeight = requested_info_header_.biHeight;
      pvi->bmiHeader.biSizeImage = GetArea(requested_info_header_) * 3 / 2;
      media_type->subtype = kMediaSubTypeI420;
      break;
    }
    case 1: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
      pvi->bmiHeader.biBitCount = 16;
      pvi->bmiHeader.biWidth = requested_info_header_.biWidth;
      pvi->bmiHeader.biHeight = requested_info_header_.biHeight;
      pvi->bmiHeader.biSizeImage = GetArea(requested_info_header_) * 2;
      media_type->subtype = MEDIASUBTYPE_YUY2;
      break;
    }
    case 2: {
      pvi->bmiHeader.biCompression = MAKEFOURCC('U', 'Y', 'V', 'Y');
      pvi->bmiHeader.biBitCount = 16;
      pvi->bmiHeader.biWidth = requested_info_header_.biWidth;
      pvi->bmiHeader.biHeight = requested_info_header_.biHeight;
      pvi->bmiHeader.biSizeImage = GetArea(requested_info_header_) * 2;
      media_type->subtype = MEDIASUBTYPE_UYVY;
      break;
    }
    case 3: {
      pvi->bmiHeader.biCompression = BI_RGB;
      pvi->bmiHeader.biBitCount = 24;
      pvi->bmiHeader.biWidth = requested_info_header_.biWidth;
      pvi->bmiHeader.biHeight = requested_info_header_.biHeight;
      pvi->bmiHeader.biSizeImage = GetArea(requested_info_header_) * 3;
      media_type->subtype = MEDIASUBTYPE_RGB24;
      break;
    }
    case 4: {
      pvi->bmiHeader.biCompression = BI_RGB;
      pvi->bmiHeader.biBitCount = 32;
      pvi->bmiHeader.biWidth = requested_info_header_.biWidth;
      pvi->bmiHeader.biHeight = requested_info_header_.biHeight;
      pvi->bmiHeader.biSizeImage = GetArea(requested_info_header_) * 4;
      media_type->subtype = MEDIASUBTYPE_RGB32;
      break;
    }
    default:
      return false;
  }

  media_type->bFixedSizeSamples = TRUE;
  media_type->lSampleSize = pvi->bmiHeader.biSizeImage;
  return true;
}

HRESULT SinkInputPin::Receive(IMediaSample* sample) {
  const int length = sample->GetActualDataLength();

  if (length <= 0 ||
      static_cast<size_t>(length) < resulting_format_.ImageAllocationSize()) {
    DLOG(WARNING) << "Wrong media sample length: " << length;
    observer_->FrameDropped(
        VideoCaptureFrameDropReason::kWinDirectShowUnexpectedSampleLength);
    return S_FALSE;
  }

  uint8_t* buffer = nullptr;
  if (FAILED(sample->GetPointer(&buffer))) {
    observer_->FrameDropped(
        VideoCaptureFrameDropReason::
            kWinDirectShowFailedToGetMemoryPointerFromMediaSample);
    return S_FALSE;
  }

  REFERENCE_TIME start_time, end_time;
  base::TimeDelta timestamp = kNoTimestamp;
  if (SUCCEEDED(sample->GetTime(&start_time, &end_time))) {
    DCHECK(start_time <= end_time);
    timestamp = base::TimeDelta::FromMicroseconds(start_time / 10);
  }

  observer_->FrameReceived(buffer, length, resulting_format_, timestamp,
                           flip_y_);
  return S_OK;
}

SinkInputPin::~SinkInputPin() {
}

}  // namespace media
