// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice.
// DirectShow is used for capturing. DirectShow provide its own threads
// for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_CAPABILITY_LIST_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_CAPABILITY_LIST_WIN_H_

#include <windows.h>

#include <list>

#include "media/capture/video_capture_types.h"

namespace media {

struct CapabilityWin {
  CapabilityWin(int media_type_index, const VideoCaptureFormat& format)
      : media_type_index(media_type_index),
        supported_format(format),
        info_header(),
        stream_index(0),
        source_pixel_format(format.pixel_format),
        maybe_fake(false) {}

  // Used by VideoCaptureDeviceWin.
  CapabilityWin(int media_type_index,
                const VideoCaptureFormat& format,
                const BITMAPINFOHEADER& info_header)
      : media_type_index(media_type_index),
        supported_format(format),
        info_header(info_header),
        stream_index(0),
        source_pixel_format(format.pixel_format),
        maybe_fake(false) {}

  // Used by VideoCaptureDeviceMFWin.
  CapabilityWin(int media_type_index,
                const VideoCaptureFormat& format,
                int stream_index,
                VideoPixelFormat source_format,
                bool maybe_fake)
      : media_type_index(media_type_index),
        supported_format(format),
        info_header(),
        stream_index(stream_index),
        source_pixel_format(source_format),
        maybe_fake(maybe_fake) {}

  const int media_type_index;
  const VideoCaptureFormat supported_format;

  // |info_header| is only valid if DirectShow is used.
  const BITMAPINFOHEADER info_header;

  // |stream_index| is only valid if MediaFoundation is used.
  const int stream_index;

  // |source_pixel_format| may differ from |supported_format|
  // if MediaFoundation is used.
  VideoPixelFormat source_pixel_format;

  // Sometimes windows exposes a fake NV12 stream, which is in fact
  // a mjpeg with decoding happening internally in MFCaptureEngine.
  bool maybe_fake;
};

typedef std::list<CapabilityWin> CapabilityList;

const CapabilityWin& GetBestMatchedCapability(
    const VideoCaptureFormat& requested,
    const CapabilityList& capabilities);

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_CAPABILITY_LIST_WIN_H_
