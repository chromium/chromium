// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory;
class VideoSourceProviderImpl;

class VideoCaptureServiceImpl : public mojom::VideoCaptureService {
 public:
  explicit VideoCaptureServiceImpl(
      mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~VideoCaptureServiceImpl() override;

  // mojom::VideoCaptureService implementation.
#if defined(OS_CHROMEOS)
  void InjectGpuDependencies(mojo::PendingRemote<mojom::AcceleratorFactory>
                                 accelerator_factory) override;
  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override;
#endif  // defined(OS_CHROMEOS)
  void ConnectToDeviceFactory(
      mojo::PendingReceiver<mojom::DeviceFactory> receiver) override;
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) override;
  void SetRetryCount(int32_t count) override;
  void BindControlsForTesting(
      mojo::PendingReceiver<mojom::TestingControls> receiver) override;

 private:
  class GpuDependenciesContext;

  void LazyInitializeGpuDependenciesContext();
  void LazyInitializeDeviceFactory();
  void LazyInitializeVideoSourceProvider();
  void OnLastSourceProviderClientDisconnected();

  mojo::Receiver<mojom::VideoCaptureService> receiver_;
  mojo::ReceiverSet<mojom::DeviceFactory> factory_receivers_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> video_source_provider_;
  std::unique_ptr<GpuDependenciesContext> gpu_dependencies_context_;

#if defined(OS_CHROMEOS)
  // Bridge for Chrome OS camera app and camera devices.
  std::unique_ptr<media::CameraAppDeviceBridgeImpl> camera_app_device_bridge_;
#endif  // defined(OS_CHROMEOS)

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureServiceImpl);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
