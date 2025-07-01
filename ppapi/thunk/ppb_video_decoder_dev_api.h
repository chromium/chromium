// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIDEO_DECODER_DEV_API_H_
#define PPAPI_THUNK_PPB_VIDEO_DECODER_DEV_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/dev/ppb_video_decoder_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_VideoDecoder_Dev_API {
 public:
  virtual ~PPB_VideoDecoder_Dev_API() {}

  virtual int32_t Decode(const PP_VideoBitstreamBuffer_Dev* bitstream_buffer,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual void AssignPictureBuffers(uint32_t no_of_buffers,
                                    const PP_PictureBuffer_Dev* buffers) = 0;
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) = 0;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Reset(scoped_refptr<TrackedCallback> callback) = 0;
  virtual void Destroy() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_VIDEO_DECODER_DEV_API_H_
