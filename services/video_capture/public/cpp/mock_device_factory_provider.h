// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_PROVIDER_H_

#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockDeviceFactoryProvider
    : public video_capture::mojom::DeviceFactoryProvider {
 public:
  MockDeviceFactoryProvider();
  ~MockDeviceFactoryProvider() override;

  void ConnectToDeviceFactory(
      video_capture::mojom::DeviceFactoryRequest request) override;

  void InjectGpuDependencies(
      video_capture::mojom::AcceleratorFactoryPtr accelerator_factory) override;

  MOCK_METHOD1(
      DoInjectGpuDependencies,
      void(video_capture::mojom::AcceleratorFactoryPtr& accelerator_factory));
  MOCK_METHOD1(SetShutdownDelayInSeconds, void(float seconds));
  MOCK_METHOD1(DoConnectToDeviceFactory,
               void(video_capture::mojom::DeviceFactoryRequest& request));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_DEVICE_FACTORY_PROVIDER_H_
