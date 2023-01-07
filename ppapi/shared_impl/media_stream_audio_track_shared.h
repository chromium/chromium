// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_MEDIA_STREAM_AUDIO_TRACK_SHARED_H_
#define PPAPI_SHARED_IMPL_MEDIA_STREAM_AUDIO_TRACK_SHARED_H_

#include <stdint.h>

#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class PPAPI_SHARED_EXPORT MediaStreamAudioTrackShared {
 public:
  struct Attributes {
    Attributes() : buffers(0), duration(0) {}
    int32_t buffers;
    int32_t duration;
  };

  static bool VerifyAttributes(const Attributes& attributes);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_MEDIA_STREAM_AUDIO_TRACK_SHARED_H_
