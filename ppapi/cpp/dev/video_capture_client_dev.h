// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_CAPTURE_CLIENT_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_CAPTURE_CLIENT_DEV_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class Instance;

class VideoCaptureClient_Dev {
 public:
  explicit VideoCaptureClient_Dev(Instance* instance);
  virtual ~VideoCaptureClient_Dev();

  virtual void OnDeviceInfo(PP_Resource video_capture,
                            const PP_VideoCaptureDeviceInfo_Dev& info,
                            const std::vector<Buffer_Dev>& buffers) = 0;
  virtual void OnStatus(PP_Resource video_capture, uint32_t status) = 0;
  virtual void OnError(PP_Resource video_capture, uint32_t error) = 0;
  virtual void OnBufferReady(PP_Resource video_capture, uint32_t buffer) = 0;

 private:
  InstanceHandle instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_CAPTURE_CLIENT_DEV_H_
