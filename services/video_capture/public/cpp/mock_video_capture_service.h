// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  MockVideoCaptureService();
  ~MockVideoCaptureService() override;

  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InjectGpuDependencies(
      mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
          accelerator_factory) override;

  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver)
      override;

  MOCK_METHOD1(
      DoInjectGpuDependencies,
      void(mojo::PendingRemote<video_capture::mojom::AcceleratorFactory>
               accelerator_factory));

  MOCK_METHOD1(DoBindVideoCaptureDeviceFactory,
               void(mojo::PendingReceiver<
                    crosapi::mojom::VideoCaptureDeviceFactory> receiver));

  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge>) override {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void BindControlsForTesting(
      mojo::PendingReceiver<mojom::TestingControls>) override {}

  MOCK_METHOD1(SetShutdownDelayInSeconds, void(float seconds));
  MOCK_METHOD1(DoConnectToVideoSourceProvider,
               void(mojo::PendingReceiver<
                    video_capture::mojom::VideoSourceProvider> receiver));

#if BUILDFLAG(IS_WIN)
  MOCK_METHOD1(OnGpuInfoUpdate, void(const CHROME_LUID&));
#endif
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_CAPTURE_SERVICE_H_
