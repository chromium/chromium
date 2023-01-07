// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIDEO_FRAME_API_H_
#define PPAPI_THUNK_PPB_VIDEO_FRAME_API_H_

#include <stdint.h>

#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

union MediaStreamBuffer;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_VideoFrame_API {
 public:
  virtual ~PPB_VideoFrame_API() {}
  virtual PP_TimeDelta GetTimestamp() = 0;
  virtual void SetTimestamp(PP_TimeDelta timestamp) = 0;
  virtual PP_VideoFrame_Format GetFormat() = 0;
  virtual PP_Bool GetSize(PP_Size* size) = 0;
  virtual void* GetDataBuffer() = 0;
  virtual uint32_t GetDataBufferSize() = 0;

  // Methods used by Pepper internal implementation only.
  virtual MediaStreamBuffer* GetBuffer() = 0;
  virtual int32_t GetBufferIndex() = 0;
  virtual void Invalidate() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_VIDEO_FRAME_API_H_
