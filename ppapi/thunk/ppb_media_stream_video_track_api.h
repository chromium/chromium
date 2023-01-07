// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_
#define PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_

#include <stdint.h>

#include "ppapi/c/ppb_media_stream_video_track.h"

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_MediaStreamVideoTrack_API {
 public:
  virtual ~PPB_MediaStreamVideoTrack_API() {}
  virtual PP_Var GetId() = 0;
  virtual PP_Bool HasEnded() = 0;
  virtual int32_t Configure(const int32_t attrib_list[],
                            scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t GetAttrib(PP_MediaStreamVideoTrack_Attrib attrib,
                            int32_t* value) = 0;
  virtual int32_t GetFrame(PP_Resource* frame,
                           scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t RecycleFrame(PP_Resource frame) = 0;
  virtual void Close() = 0;
  virtual int32_t GetEmptyFrame(
      PP_Resource* frame,
      scoped_refptr<ppapi::TrackedCallback> callback) = 0;
  virtual int32_t PutFrame(PP_Resource frame) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_MEDIA_STREAM_VIDEO_TRACK_API_H_
