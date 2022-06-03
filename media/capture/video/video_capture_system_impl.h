// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_

#include "media/capture/video/video_capture_system.h"

namespace media {

// Layer on top of VideoCaptureDeviceFactory that translates device descriptors
// to string identifiers and consolidates and caches device descriptors and
// supported formats into VideoCaptureDeviceInfos.
class CAPTURE_EXPORT VideoCaptureSystemImpl : public VideoCaptureSystem {
 public:
  explicit VideoCaptureSystemImpl(
      std::unique_ptr<VideoCaptureDeviceFactory> factory);
  ~VideoCaptureSystemImpl() override;

  void GetDeviceInfosAsync(DeviceInfoCallback result_callback) override;
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const std::string& device_id) override;

 private:
  using DeviceEnumQueue = std::list<DeviceInfoCallback>;

  // Returns nullptr if no descriptor found.
  const VideoCaptureDeviceInfo* LookupDeviceInfoFromId(
      const std::string& device_id);

  void DevicesInfoReady(std::vector<VideoCaptureDeviceInfo> devices_info);

  const std::unique_ptr<VideoCaptureDeviceFactory> factory_;
  DeviceEnumQueue device_enum_request_queue_;
  std::vector<VideoCaptureDeviceInfo> devices_info_cache_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<VideoCaptureSystemImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_IMPL_H_
