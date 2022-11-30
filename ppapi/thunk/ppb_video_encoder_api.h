// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_VIDEO_ENCODER_API_H_
#define PPAPI_THUNK_PPB_VIDEO_ENCODER_API_H_

#include <stdint.h>

#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/ppb_video_encoder.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_VideoEncoder_API {
 public:
  virtual ~PPB_VideoEncoder_API() {}

  virtual int32_t GetSupportedProfiles(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t GetSupportedProfiles0_1(
      const PP_ArrayOutput& output,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t Initialize(
      PP_VideoFrame_Format input_format,
      const PP_Size* input_visible_size,
      PP_VideoProfile output_profile,
      uint32_t initial_bitrate,
      PP_HardwareAcceleration acceleration,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t GetFramesRequired() = 0;
  virtual int32_t GetFrameCodedSize(PP_Size* size) = 0;
  virtual int32_t GetVideoFrame(
      PP_Resource* video_frame,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t Encode(PP_Resource video_frame,
                         PP_Bool force_keyframe,
                         const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual int32_t GetBitstreamBuffer(
      PP_BitstreamBuffer* bitstream_buffer,
      const scoped_refptr<TrackedCallback>& callback) = 0;
  virtual void RecycleBitstreamBuffer(
      const PP_BitstreamBuffer* bitstream_buffer) = 0;
  virtual void RequestEncodingParametersChange(uint32_t bitrate,
                                               uint32_t framerate) = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_VIDEO_ENCODER_API_H_
