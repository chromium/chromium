// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
#define PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class VideoCapture_Dev : public Resource {
 public:
  explicit VideoCapture_Dev(const InstanceHandle& instance);
  VideoCapture_Dev(PP_Resource resource);

  virtual ~VideoCapture_Dev();

  // Returns true if the required interface is available.
  static bool IsAvailable();

  int32_t EnumerateDevices(
      const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >&
          callback);
  int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                              void* user_data);
  int32_t Open(const DeviceRef_Dev& device_ref,
               const PP_VideoCaptureDeviceInfo_Dev& requested_info,
               uint32_t buffer_count,
               const CompletionCallback& callback);
  int32_t StartCapture();
  int32_t ReuseBuffer(uint32_t buffer);
  int32_t StopCapture();
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_VIDEO_CAPTURE_DEV_H_
