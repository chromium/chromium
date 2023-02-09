// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "services/video_capture/device_factory_impl.h"
#include "services/video_capture/testing_controls_impl.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"
#include "services/viz/public/cpp/gpu/gpu.h"

#if BUILDFLAG(IS_MAC)
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "services/video_capture/lacros/device_factory_adapter_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace video_capture {

// Intended usage of this class is to instantiate on any sequence, and then
// operate and release the instance on the task runner exposed via
// GetTaskRunner() via WeakPtrs provided via GetWeakPtr(). To this end,
// GetTaskRunner() and GetWeakPtr() can be called from any sequence, typically
// the same as the one calling the constructor.
class VideoCaptureServiceImpl::GpuDependenciesContext {
 public:
  GpuDependenciesContext() {
    gpu_io_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING, base::MayBlock()});
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  // Task runner for operating |accelerator_factory_| and
  // |gpu_memory_buffer_manager_| on. This must be a different thread from the
  // main service thread in order to avoid a deadlock during shutdown where
  // the main service thread joins a video capture device thread that, in turn,
  // will try to post the release of the jpeg decoder to the thread it is
  // operated on.
  scoped_refptr<base::SequencedTaskRunner> gpu_io_task_runner_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::Remote<mojom::AcceleratorFactory> accelerator_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::WeakPtrFactory<GpuDependenciesContext> weak_factory_for_gpu_io_thread_{
      this};
};

VideoCaptureServiceImpl::VideoCaptureServiceImpl(
    mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : receiver_(this, std::move(receiver)),
      ui_task_runner_(std::move(ui_task_runner)) {}

VideoCaptureServiceImpl::~VideoCaptureServiceImpl() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  factory_receivers_ash_.Clear();
  device_factory_ash_adapter_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  device_factory_.reset();

  if (gpu_dependencies_context_) {
    gpu_dependencies_context_->GetTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(gpu_dependencies_context_));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  LazyInitializeDeviceFactory();
  media::CameraAppDeviceBridgeImpl::GetInstance()->BindReceiver(
      std::move(receiver));
}

void VideoCaptureServiceImpl::BindVideoCaptureDeviceFactory(
    mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver) {
  LazyInitializeDeviceFactory();
  factory_receivers_ash_.Add(device_factory_ash_adapter_.get(),
                             std::move(receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void VideoCaptureServiceImpl::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) {
  LazyInitializeVideoSourceProvider();
  video_source_provider_->AddClient(std::move(receiver));
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
  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::CreateVideoCaptureDeviceFactory(ui_task_runner_);

  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryImpl>(
          std::move(video_capture_system),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          gpu_dependencies_context_->GetTaskRunner()));
  device_factory_ash_adapter_ =
      std::make_unique<crosapi::VideoCaptureDeviceFactoryAsh>(
          device_factory_.get());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // LacrosService might be null in unit tests.
  auto* lacros_service = chromeos::LacrosService::Get();

  // For requests for fake (including file) video capture device factory, we
  // don't need to forward the request to Ash-Chrome.
  if (!media::ShouldUseFakeVideoCaptureDeviceFactory() && lacros_service &&
      lacros_service->IsVideoCaptureDeviceFactoryAvailable()) {
    mojo::PendingRemote<crosapi::mojom::VideoCaptureDeviceFactory>
        device_factory_ash;
    lacros_service->BindVideoCaptureDeviceFactory(
        device_factory_ash.InitWithNewPipeAndPassReceiver());
    device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
        std::make_unique<DeviceFactoryAdapterLacros>(
            std::move(device_factory_ash)));
  } else {
    LOG(WARNING)
        << "Connected to an older version of ash. Use device factory in "
           "Lacros-Chrome which is backed by Linux VCD instead of CrOS VCD.";
    device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
        std::make_unique<DeviceFactoryImpl>(std::move(video_capture_system)));
  }
#else
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryImpl>(std::move(video_capture_system)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

#if BUILDFLAG(IS_WIN)
void VideoCaptureServiceImpl::OnGpuInfoUpdate(const CHROME_LUID& luid) {
  LazyInitializeDeviceFactory();
  device_factory_->OnGpuInfoUpdate(luid);
}
#endif

}  // namespace video_capture
