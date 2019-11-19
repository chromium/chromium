// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/testing_controls_impl.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"
#include "services/viz/public/cpp/gpu/gpu.h"

#if defined(OS_MACOSX)
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#endif

namespace video_capture {

// Intended usage of this class is to instantiate on any sequence, and then
// operate and release the instance on the task runner exposed via
// GetTaskRunner() via WeakPtrs provided via GetWeakPtr(). To this end,
// GetTaskRunner() and GetWeakPtr() can be called from any sequence, typically
// the same as the one calling the constructor.
class VideoCaptureServiceImpl::GpuDependenciesContext {
 public:
  GpuDependenciesContext() {
    gpu_io_task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::TaskPriority::BEST_EFFORT,
         base::MayBlock()});
  }

  ~GpuDependenciesContext() {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
  }

  base::WeakPtr<GpuDependenciesContext> GetWeakPtr() {
    return weak_factory_for_gpu_io_thread_.GetWeakPtr();
  }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return gpu_io_task_runner_;
  }

#if defined(OS_CHROMEOS)
  void InjectGpuDependencies(
      mojo::PendingRemote<mojom::AcceleratorFactory> accelerator_factory_info) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    accelerator_factory_.reset();
    accelerator_factory_.Bind(std::move(accelerator_factory_info));
  }

  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          receiver) {
    DCHECK(gpu_io_task_runner_->RunsTasksInCurrentSequence());
    if (!accelerator_factory_)
      return;
    accelerator_factory_->CreateJpegDecodeAccelerator(std::move(receiver));
  }
#endif  // defined(OS_CHROMEOS)

 private:
  // Task runner for operating |accelerator_factory_| and
  // |gpu_memory_buffer_manager_| on. This must be a different thread from the
  // main service thread in order to avoid a deadlock during shutdown where
  // the main service thread joins a video capture device thread that, in turn,
  // will try to post the release of the jpeg decoder to the thread it is
  // operated on.
  scoped_refptr<base::SequencedTaskRunner> gpu_io_task_runner_;

#if defined(OS_CHROMEOS)
  mojo::Remote<mojom::AcceleratorFactory> accelerator_factory_;
#endif  // defined(OS_CHROMEOS)

  base::WeakPtrFactory<GpuDependenciesContext> weak_factory_for_gpu_io_thread_{
      this};
};

VideoCaptureServiceImpl::VideoCaptureServiceImpl(
    mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : receiver_(this, std::move(receiver)),
      ui_task_runner_(std::move(ui_task_runner)) {}

VideoCaptureServiceImpl::~VideoCaptureServiceImpl() {
  factory_receivers_.Clear();
  device_factory_.reset();

#if defined(OS_CHROMEOS)
  camera_app_device_bridge_.reset();
#endif  // defined (OS_CHROMEOS)

  if (gpu_dependencies_context_) {
    gpu_dependencies_context_->GetTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(gpu_dependencies_context_));
  }
}

#if defined(OS_CHROMEOS)
void VideoCaptureServiceImpl::InjectGpuDependencies(
    mojo::PendingRemote<mojom::AcceleratorFactory> accelerator_factory) {
  LazyInitializeGpuDependenciesContext();
  gpu_dependencies_context_->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GpuDependenciesContext::InjectGpuDependencies,
                                gpu_dependencies_context_->GetWeakPtr(),
                                std::move(accelerator_factory)));
}

void VideoCaptureServiceImpl::ConnectToCameraAppDeviceBridge(
    mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver) {
  DCHECK(camera_app_device_bridge_);
  camera_app_device_bridge_->BindReceiver(std::move(receiver));
}
#endif  // defined(OS_CHROMEOS)

void VideoCaptureServiceImpl::ConnectToDeviceFactory(
    mojo::PendingReceiver<mojom::DeviceFactory> receiver) {
  LazyInitializeDeviceFactory();
  factory_receivers_.Add(device_factory_.get(), std::move(receiver));
}

void VideoCaptureServiceImpl::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) {
  LazyInitializeVideoSourceProvider();
  video_source_provider_->AddClient(std::move(receiver));
}

void VideoCaptureServiceImpl::SetRetryCount(int32_t count) {
#if defined(OS_MACOSX)
  media::VideoCaptureDeviceFactoryMac::SetGetDeviceDescriptorsRetryCount(count);
#endif
}

void VideoCaptureServiceImpl::BindControlsForTesting(
    mojo::PendingReceiver<mojom::TestingControls> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<TestingControlsImpl>(),
                              std::move(receiver));
}

void VideoCaptureServiceImpl::LazyInitializeGpuDependenciesContext() {
  if (!gpu_dependencies_context_)
    gpu_dependencies_context_ = std::make_unique<GpuDependenciesContext>();
}

void VideoCaptureServiceImpl::LazyInitializeDeviceFactory() {
  if (device_factory_)
    return;

  LazyInitializeGpuDependenciesContext();

  // Create the platform-specific device factory.
  // The task runner passed to CreateFactory is used for things that need to
  // happen on a "UI thread equivalent", e.g. obtaining screen rotation on
  // Chrome OS.
#if defined(OS_CHROMEOS)
  camera_app_device_bridge_ =
      std::make_unique<media::CameraAppDeviceBridgeImpl>();
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::CreateVideoCaptureDeviceFactory(ui_task_runner_,
                                             camera_app_device_bridge_.get());
  camera_app_device_bridge_->SetIsSupported(
      media_device_factory->IsSupportedCameraAppDeviceBridge());
#else
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::CreateVideoCaptureDeviceFactory(ui_task_runner_);
#endif  // defined(OS_CHROMEOS)

  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

#if defined(OS_CHROMEOS)
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          std::move(video_capture_system),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          gpu_dependencies_context_->GetTaskRunner()));
#else
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryMediaToMojoAdapter>(
          std::move(video_capture_system)));
#endif  // defined(OS_CHROMEOS)
}

void VideoCaptureServiceImpl::LazyInitializeVideoSourceProvider() {
  if (video_source_provider_)
    return;
  LazyInitializeDeviceFactory();
  video_source_provider_ = std::make_unique<VideoSourceProviderImpl>(
      device_factory_.get(),
      // Unretained(this) is safe, because |this| owns |video_source_provider_|.
      base::BindRepeating(
          &VideoCaptureServiceImpl::OnLastSourceProviderClientDisconnected,
          base::Unretained(this)));
}

void VideoCaptureServiceImpl::OnLastSourceProviderClientDisconnected() {
  video_source_provider_.reset();
}

}  // namespace video_capture
