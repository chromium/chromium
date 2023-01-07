// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_MEDIA_STREAM_AUDIO_TRACK_API_H_
#define PPAPI_THUNK_PPB_MEDIA_STREAM_AUDIO_TRACK_API_H_

#include <stdint.h>

#include "ppapi/c/ppb_media_stream_audio_track.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_MediaStreamAudioTrack_API {
 public:
  virtual ~PPB_MediaStreamAudioTrack_API() {}
  virtual PP_Var GetId() = 0;
  virtual PP_Bool HasEnded() = 0;
  virtual int32_t Configure(const int32_t attrib_list[],
                            scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t GetAttrib(PP_MediaStreamAudioTrack_Attrib attrib,
                            int32_t* value) = 0;
  virtual int32_t GetBuffer(PP_Resource* buffer,
                            scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t RecycleBuffer(PP_Resource buffer) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_MEDIA_STREAM_AUDIO_TRACK_API_H_
