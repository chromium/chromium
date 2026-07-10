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
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/ipc/client/gpu_channel_observer.h"
#include "media/capture/video/create_video_capture_device_factory.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_capture/device_factory_impl.h"
#include "services/video_capture/public/cpp/features.h"
#include "services/video_capture/testing_controls_impl.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "services/video_capture/virtual_device_enabled_device_factory.h"
#include "services/viz/public/cpp/gpu/gpu.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
#if BUILDFLAG(IS_ANDROID)
#include "base/task/bind_post_task.h"
#include "gpu/ipc/client/gpu_channel_observer.h"
#endif
#include "media/capture/capture_switches.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

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

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  // Task runner for operating |accelerator_factory_| and
  // |shared_image_interface_| on. This must be a different thread from the
  // main service thread in order to avoid a deadlock during shutdown where
  // the main service thread joins a video capture device thread that, in turn,
  // will try to post the release of the jpeg decoder to the thread it is
  // operated on.
  scoped_refptr<base::SequencedTaskRunner> gpu_io_task_runner_;

#if BUILDFLAG(IS_CHROMEOS)
  mojo::Remote<mojom::AcceleratorFactory> accelerator_factory_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::WeakPtrFactory<GpuDependenciesContext> weak_factory_for_gpu_io_thread_{
      this};
};

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
// Intended usage of this class is to create viz::Gpu in utility process and
// connect to viz::GpuClient of browser process, which will call to Gpu service.
// Also, this class holds the viz::ContextProvider to listen and monitor Gpu
// context lost event. The viz::Gpu and viz::ContextProvider need be created in
// the main thread of utility process. The |main_task_runner_| is initialized as
// the default single thread task runner of main thread. The
// viz::ContextProvider will call BindToCurrentSequence on |main_task_runner_|
// sequence of main thread. Then, the gpu context lost event will be called in
// the |main_task_runner_| sequence, which will be notified to the
// media::VideoCaptureGpuChannelHost.
class VideoCaptureServiceImpl::VizGpuContextProvider
    : public gpu::GpuChannelLostObserver {
 public:
  VizGpuContextProvider(std::unique_ptr<viz::Gpu> viz_gpu)
      : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        viz_gpu_(std::move(viz_gpu)) {
    StartContextProviderIfNeeded();
  }

  virtual ~VizGpuContextProvider() {
    DCHECK(main_task_runner_->BelongsToCurrentThread());
    // Ensure destroy context provider and not receive callbacks before clear up
    // |viz_gpu_|.
    if (shared_image_interface_) {
      // Ensure there are no dangling pointers.
      media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
          nullptr);
      media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(nullptr);
#if BUILDFLAG(IS_CHROMEOS)
      media::VideoCaptureDeviceFactoryChromeOS::SetGpuChannelHost(nullptr);
      media::VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(
          nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)
      shared_image_interface_->RemoveGpuChannelLostObserver(this);
      shared_image_interface_.reset();
    }
  }

  // gpu::GpuChannelLostObserver implementation.
  void OnGpuChannelLost() override {
    // GpuChannelHost automatically unsubscribes observers, we don't need to do
    // it here.
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VizGpuContextProvider::OnGpuChannelLostOnMainThread,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGpuChannelLostOnMainThread() {
    shared_image_interface_.reset();
    StartContextProviderIfNeeded();

    // Notify context lost after new context ready.
    media::VideoCaptureGpuChannelHost::GetInstance().OnContextLost();
  }

  gpu::GpuDriverBugWorkarounds GetGpuDriverBugWorkarounds() {
    if (!viz_gpu_) {
      return gpu::GpuDriverBugWorkarounds();
    }
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
        viz_gpu_->GetGpuChannel();
    if (!gpu_channel_host) {
      return gpu::GpuDriverBugWorkarounds();
    }
    return gpu::GpuDriverBugWorkarounds(
        gpu_channel_host->gpu_feature_info()
            .enabled_gpu_driver_bug_workarounds);
  }

 private:
  void StartContextProviderIfNeeded() {
    DCHECK_EQ(shared_image_interface_, nullptr);
    DCHECK(main_task_runner_->BelongsToCurrentThread());

    // Reset GpuChannelHost and related objects to begin
    // with. Set it back when GpuChannelHost is created/re-created successfully.
    media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        nullptr);
    media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(nullptr);
#if BUILDFLAG(IS_CHROMEOS)
    media::VideoCaptureDeviceFactoryChromeOS::SetGpuChannelHost(nullptr);
    media::VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS)

    if (!viz_gpu_) {
      return;
    }

    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
        viz_gpu_->GetGpuChannel();
    if (!gpu_channel_host || gpu_channel_host->IsLost()) {
      gpu_channel_host = viz_gpu_->EstablishGpuChannelSync();
    }

    if (!gpu_channel_host) {
      return;
    }

    shared_image_interface_ =
        gpu_channel_host->CreateClientSharedImageInterface();

    if (!shared_image_interface_->AddGpuChannelLostObserver(this)) {
      shared_image_interface_.reset();
      LOG(ERROR) << "Context already lost.";
      return;
    }

    media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(
        viz_gpu_->GetGpuChannel());
    media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        shared_image_interface_);
#if BUILDFLAG(IS_CHROMEOS)
    media::VideoCaptureDeviceFactoryChromeOS::SetGpuChannelHost(
        viz_gpu_->GetGpuChannel());
    media::VideoCaptureDeviceFactoryChromeOS::SetSharedImageInterface(
        shared_image_interface_);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  // Task runner for operating |viz_gpu_|. This must be the main service thread
  // as the |viz_gpu_| required.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<viz::Gpu> viz_gpu_;
  scoped_refptr<gpu::SharedImageInterface> shared_image_interface_;
  base::WeakPtrFactory<VizGpuContextProvider> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
// TODO(crbug.com/417807138): `gpu_workarounds` must be ready before creating
// `VideoCaptureDeviceFactoryAndroid` to avoid race conditions with incoming
// Mojo requests. Thus, the initial `GpuChannelHost` is established in the
// browser process and passed in synchronously during service initialization,
// rather than being fetched asynchronously within the service. We should
// eventually make Android VCD adapt to asynchronously fetching
// `gpu_workarounds` so that we can handle all GPU channel initialization
// internally within the service.
//
// This class has to be freed on the UI thread.
class VideoCaptureServiceImpl::BrowserGpuChannelHostProvider final
    : public gpu::GpuChannelLostObserver {
 public:
  BrowserGpuChannelHostProvider(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      GpuChannelHostBinder binder)
      : ui_task_runner_(std::move(ui_task_runner)),
        gpu_channel_host_(std::move(gpu_channel_host)),
        gpu_channel_host_binder_(std::move(binder)),
        gpu_driver_bug_workarounds_(
            gpu_channel_host_ ? gpu::GpuDriverBugWorkarounds(
                                    gpu_channel_host_->gpu_feature_info()
                                        .enabled_gpu_driver_bug_workarounds)
                              : gpu::GpuDriverBugWorkarounds()) {
    StartGpuChannelHostIfNeeded();
  }

  ~BrowserGpuChannelHostProvider() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());

    if (gpu_channel_host_) {
      gpu_channel_host_->RemoveObserver(this);
      media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
          nullptr);
      media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(nullptr);
    }
  }

  // gpu::GpuChannelLostObserver implementation.
  void OnGpuChannelLost() override {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserGpuChannelHostProvider::OnGpuChannelLostOnUIThread,
            weak_ptr_factory_.GetWeakPtr()));
  }

  gpu::GpuDriverBugWorkarounds GetGpuDriverBugWorkarounds() const {
    return gpu_driver_bug_workarounds_;
  }

 private:
  void OnGpuChannelLostOnUIThread() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());

    if (gpu_channel_host_) {
      gpu_channel_host_->RemoveObserver(this);
      gpu_channel_host_ = nullptr;
    }

    StartGpuChannelHostIfNeeded();
  }

  void StartGpuChannelHostIfNeeded() {
    if (!ui_task_runner_->BelongsToCurrentThread()) {
      ui_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BrowserGpuChannelHostProvider::StartGpuChannelHostIfNeeded,
              weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        nullptr);
    media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(nullptr);

    if (gpu_channel_host_ && !gpu_channel_host_->IsLost()) {
      OnGpuChannelEstablished(gpu_channel_host_);
      return;
    }

    gpu_channel_host_binder_.Run(
        base::BindOnce(&BrowserGpuChannelHostProvider::OnGpuChannelEstablished,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());

    gpu_channel_host_ = std::move(gpu_channel_host);
    if (gpu_channel_host_) {
      (void)gpu_channel_host_->AddObserverIfNotAlreadyLost(this);
      media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
          gpu_channel_host_->CreateClientSharedImageInterface());
      media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(
          gpu_channel_host_);
    } else {
      media::VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
          nullptr);
      media::VideoCaptureGpuChannelHost::GetInstance().SetGpuChannel(nullptr);
    }

    media::VideoCaptureGpuChannelHost::GetInstance().OnContextLost();
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  GpuChannelHostBinder gpu_channel_host_binder_;
  const gpu::GpuDriverBugWorkarounds gpu_driver_bug_workarounds_;
  base::WeakPtrFactory<BrowserGpuChannelHostProvider> weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

VideoCaptureServiceImpl::VideoCaptureServiceImpl(
    mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool create_system_monitor)
    : receiver_(this, std::move(receiver)),
      ui_task_runner_(std::move(ui_task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (create_system_monitor && !base::SystemMonitor::Get()) {
    system_monitor_ = std::make_unique<base::SystemMonitor>();
  }
#if BUILDFLAG(IS_MAC)
    InitializeDeviceMonitor();
#endif
#if BUILDFLAG(IS_CHROMEOS)
    media::CameraAppDeviceBridgeImpl::GetInstance()->SetUITaskRunner(
        ui_task_runner_);
#endif
#if BUILDFLAG(IS_WIN)
    if (base::FeatureList::IsEnabled(
            features::kWinCameraMonitoringInVideoCaptureService)) {
      InitializeDeviceMonitor();
    }
#endif
}

VideoCaptureServiceImpl::~VideoCaptureServiceImpl() {
  device_factory_.reset();

  if (gpu_dependencies_context_) {
    gpu_dependencies_context_->GetTaskRunner()->DeleteSoon(
        FROM_HERE, std::move(gpu_dependencies_context_));
  }

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  if (browser_gpu_channel_host_provider_) {
    ui_task_runner_->DeleteSoon(FROM_HERE,
                                std::move(browser_gpu_channel_host_provider_));
  }
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!gpu_dependencies_context_)
    gpu_dependencies_context_ = std::make_unique<GpuDependenciesContext>();

    // Gpu channel is enabled on all platforms except Lacros.
#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  // We don't need to initialize VizGpuContextProvider if viz_gpu_ is null, as
  // it has no functionality without a viz::Gpu instance.
  if (!viz_gpu_context_provider_ && viz_gpu_) {
    viz_gpu_context_provider_ =
        std::make_unique<VizGpuContextProvider>(std::move(viz_gpu_));
  }
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
}

void VideoCaptureServiceImpl::LazyInitializeDeviceFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (device_factory_)
    return;

  LazyInitializeGpuDependenciesContext();

  // Create the platform-specific device factory.
  // The task runner passed to CreateFactory is used for things that need to
  // happen on a "UI thread equivalent", e.g. obtaining screen rotation on
  // Chrome OS.
  gpu::GpuDriverBugWorkarounds* gpu_workarounds_ptr = nullptr;
#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
  gpu::GpuDriverBugWorkarounds gpu_workarounds;
#if BUILDFLAG(IS_ANDROID)
  if (browser_gpu_channel_host_provider_) {
    gpu_workarounds =
        browser_gpu_channel_host_provider_->GetGpuDriverBugWorkarounds();
  } else if (viz_gpu_context_provider_) {
#else
  if (viz_gpu_context_provider_) {
#endif  // BUILDFLAG(IS_ANDROID)
    gpu_workarounds = viz_gpu_context_provider_->GetGpuDriverBugWorkarounds();
  }
  gpu_workarounds_ptr = &gpu_workarounds;

#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

  std::unique_ptr<media::VideoCaptureDeviceFactory> media_device_factory =
      media::CreateVideoCaptureDeviceFactory(ui_task_runner_,
                                             gpu_workarounds_ptr);

  auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
      std::move(media_device_factory));

#if BUILDFLAG(IS_CHROMEOS)
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryImpl>(
          std::move(video_capture_system),
          base::BindRepeating(
              &GpuDependenciesContext::CreateJpegDecodeAccelerator,
              gpu_dependencies_context_->GetWeakPtr()),
          gpu_dependencies_context_->GetTaskRunner()));
#else
  device_factory_ = std::make_unique<VirtualDeviceEnabledDeviceFactory>(
      std::make_unique<DeviceFactoryImpl>(std::move(video_capture_system)));
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void VideoCaptureServiceImpl::LazyInitializeVideoSourceProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

void VideoCaptureServiceImpl::InitializeDeviceMonitor() {
#if BUILDFLAG(IS_MAC)
  if (video_capture_device_monitor_mac_) {
    return;
  }
  video_capture_device_monitor_mac_ = std::make_unique<media::DeviceMonitorMac>(
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_VISIBLE}));
  video_capture_device_monitor_mac_->StartMonitoring();
#endif

#if BUILDFLAG(IS_WIN)
  CHECK(base::FeatureList::IsEnabled(
      features::kWinCameraMonitoringInVideoCaptureService));
  if (video_capture_system_message_window_win_) {
    return;
  }
  video_capture_system_message_window_win_ =
      std::make_unique<media::SystemMessageWindowWin>();
#endif
}

#if BUILDFLAG(IS_WIN)
void VideoCaptureServiceImpl::OnGpuInfoUpdate(const CHROME_LUID& luid) {
  LazyInitializeDeviceFactory();
  device_factory_->OnGpuInfoUpdate(luid);
}
#endif

#if BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)
void VideoCaptureServiceImpl::SetVizGpu(std::unique_ptr<viz::Gpu> viz_gpu) {
  viz_gpu_ = std::move(viz_gpu);
}

#if BUILDFLAG(IS_ANDROID)
void VideoCaptureServiceImpl::SetGpuChannelHost(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    GpuChannelHostBinder binder) {
  browser_gpu_channel_host_provider_ =
      std::make_unique<BrowserGpuChannelHostProvider>(
          ui_task_runner_, std::move(gpu_channel_host), std::move(binder));
}
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_GPU_CHANNEL_MEDIA_CAPTURE)

}  // namespace video_capture
