// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  MockVideoCaptureService();
  ~MockVideoCaptureService() override;

  void ConnectToDeviceFactory(
      mojo::PendingReceiver<video_capture::mojom::DeviceFactory> receiver)
      override;

  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;

#if defined(OS_CHROMEOS)
  void InjectGpuDependencies(
      mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
          accelerator_factory) override;

  MOCK_METHOD1(
      DoInjectGpuDependencies,
      void(mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
               accelerator_factory));

  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge>) override {}
#endif  // defined(OS_CHROMEOS

  void BindControlsForTesting(
      mojo::PendingReceiver<mojom::TestingControls>) override {}

  MOCK_METHOD1(SetShutdownDelayInSeconds, void(float seconds));
  MOCK_METHOD1(DoConnectToDeviceFactory,
               void(mojo::PendingReceiver<video_capture::mojom::DeviceFactory>
                        receiver));
  MOCK_METHOD1(DoConnectToVideoSourceProvider,
               void(mojo::PendingReceiver<
                    video_capture::mojom::VideoSourceProvider> receiver));
  MOCK_METHOD1(SetRetryCount, void(int32_t));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_
