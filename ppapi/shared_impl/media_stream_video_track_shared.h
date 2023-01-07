// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_MEDIA_STREAM_VIDEO_TRACK_SHARED_H_
#define PPAPI_SHARED_IMPL_MEDIA_STREAM_VIDEO_TRACK_SHARED_H_

#include <stdint.h>

#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT MediaStreamVideoTrackShared {
 public:
  struct Attributes {
    Attributes()
        : buffers(0),
          width(0),
          height(0),
          format(PP_VIDEOFRAME_FORMAT_UNKNOWN) {}
    int32_t buffers;
    int32_t width;
    int32_t height;
    PP_VideoFrame_Format format;
  };

  static bool VerifyAttributes(const Attributes& attributes);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_MEDIA_STREAM_VIDEO_TRACK_SHARED_H_
