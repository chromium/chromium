// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "services/video_capture/device.h"
#include "services/video_capture/public/mojom/device_factory.mojom-shared.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"

namespace video_capture {

class DeviceFactory : public mojom::DeviceFactory {
 public:
  struct DeviceInProcessInfo {
    raw_ptr<Device> device;
    media::VideoCaptureError result_code;
  };
  using CreateDeviceInProcessCallback =
      base::OnceCallback<void(DeviceInProcessInfo)>;
  virtual void CreateDeviceInProcess(const std::string& device_id,
                                     CreateDeviceInProcessCallback callback) {}

  virtual void StopDeviceInProcess(const std::string device_id) {}
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
