// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_child_thread.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/child/child_process.h"
#include "content/common/process_visibility_tracker.h"
#include "content/gpu/browser_exposed_gpu_interfaces.h"
#include "content/gpu/gpu_service_factory.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/scoped_message_error_crash_key.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "third_party/skia/include/core/SkGraphics.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_drm_bridge_client.h"
#include "media/mojo/clients/mojo_android_overlay.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/services/font/public/cpp/font_loader.h"  // nogncheck
#include "components/services/font/public/mojom/font_service.mojom.h"  // nogncheck
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/sandboxed_process_thread_type_handler.h"
#endif

namespace content {
namespace {

// Called when the GPU process receives a bad IPC message.
void HandleBadMessage(const std::string& error) {
  LOG(ERROR) << "Mojo error in GPU process: " << error;
  mojo::debug::ScopedMessageErrorCrashKey crash_key_value(error);
  base::debug::DumpWithoutCrashing();
}

ChildThreadImpl::Options GetOptions(
    const InProcessChildThreadParams* in_process_params = nullptr) {
  ChildThreadImpl::Options::Builder builder;
  builder.ConnectToBrowser(true);
  builder.ExposesInterfacesToBrowser();
  if (in_process_params)
    builder.InBrowserProcess(*in_process_params);
  return builder.Build();
}

viz::VizMainImpl::ExternalDependencies CreateVizMainDependencies() {
  viz::VizMainImpl::ExternalDependencies deps;
  if (!base::PowerMonitor::GetInstance()->IsInitialized()) {
    deps.power_monitor_source =
        std::make_unique<base::PowerMonitorDeviceSource>();
  }
  if (GetContentClient()->gpu()) {
    deps.sync_point_manager = GetContentClient()->gpu()->GetSyncPointManager();
    deps.shared_image_manager =
        GetContentClient()->gpu()->GetSharedImageManager();
    deps.scheduler = GetContentClient()->gpu()->GetScheduler();
    deps.viz_compositor_thread_runner =
        GetContentClient()->gpu()->GetVizCompositorThreadRunner();
  }
  auto* process = ChildProcess::current();
  deps.shutdown_event = process->GetShutDownEvent();
  deps.io_thread_task_runner = process->io_task_runner();

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  ChildThread::Get()->BindHostReceiver(factory.BindNewPipeAndPassReceiver());
  deps.ukm_recorder = ukm::MojoUkmRecorder::Create(*factory);
  return deps;
}

}  // namespace

GpuChildThread::GpuChildThread(base::RepeatingClosure quit_closure,
                               std::unique_ptr<gpu::GpuInit> gpu_init)
    : GpuChildThread(std::move(quit_closure),
                     GetOptions(),
                     std::move(gpu_init)) {}

GpuChildThread::GpuChildThread(const InProcessChildThreadParams& params,
                               std::unique_ptr<gpu::GpuInit> gpu_init)
    : GpuChildThread(base::DoNothing(),
                     GetOptions(&params),
                     std::move(gpu_init)) {}

GpuChildThread::GpuChildThread(base::RepeatingClosure quit_closure,
                               ChildThreadImpl::Options options,
                               std::unique_ptr<gpu::GpuInit> gpu_init)
    : ChildThreadImpl(MakeQuitSafelyClosure(), std::move(options)),
      viz_main_(this, CreateVizMainDependencies(), std::move(gpu_init)),
      quit_closure_(std::move(quit_closure)) {
  if (in_process_gpu()) {
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kSingleProcess) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               switches::kInProcessGPU));
  }
}

GpuChildThread::~GpuChildThread() = default;

void GpuChildThread::Init(const base::TimeTicks& process_start_time) {
  if (!in_process_gpu())
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(&HandleBadMessage));

  viz_main_.gpu_service()->set_start_time(process_start_time);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  SandboxedProcessThreadTypeHandler::NotifyMainChildThreadCreated();
#endif

  // When running in in-process mode, this has been set in the browser at
  // ChromeBrowserMainPartsAndroid::PreMainMessageLoopRun().
#if BUILDFLAG(IS_ANDROID)
  if (!in_process_gpu()) {
    media::SetMediaDrmBridgeClient(
        GetContentClient()->GetMediaDrmBridgeClient());
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!in_process_gpu()) {
    mojo::PendingRemote<font_service::mojom::FontService> font_service;
    BindHostReceiver(font_service.InitWithNewPipeAndPassReceiver());
    SkFontConfigInterface::SetGlobal(
        sk_make_sp<font_service::FontLoader>(std::move(font_service)));
  }
#endif

  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&GpuChildThread::OnMemoryPressure,
                                     base::Unretained(this)));
}

bool GpuChildThread::in_process_gpu() const {
  return viz_main_.gpu_service()->gpu_info().in_process_gpu;
}

void GpuChildThread::OnInitializationFailed() {
  OnChannelError();
}

void GpuChildThread::OnGpuServiceConnection(viz::GpuServiceImpl* gpu_service) {
  media::AndroidOverlayMojoFactoryCB overlay_factory_cb;
#if BUILDFLAG(IS_ANDROID)
  overlay_factory_cb =
      base::BindRepeating(&GpuChildThread::CreateAndroidOverlay,
                          base::SingleThreadTaskRunner::GetCurrentDefault());
  gpu_service->media_gpu_channel_manager()->SetOverlayFactory(
      overlay_factory_cb);
#endif

  if (!IsInBrowserProcess()) {
    gpu_service->SetVisibilityChangedCallback(
        base::BindRepeating([](bool visible) {
          ProcessVisibilityTracker::GetInstance()->OnProcessVisibilityChanged(
              visible);
        }));
  }

  // Only set once per process instance.
  service_factory_ = std::make_unique<GpuServiceFactory>(
      gpu_service->gpu_preferences(),
      gpu_service->gpu_channel_manager()->gpu_driver_bug_workarounds(),
      gpu_service->gpu_feature_info(), gpu_service->gpu_info(),
      gpu_service->media_gpu_channel_manager()->AsWeakPtr(),
      gpu_service->gpu_memory_buffer_factory(), std::move(overlay_factory_cb));
  for (auto& receiver : pending_service_receivers_)
    BindServiceInterface(std::move(receiver));
  pending_service_receivers_.clear();

  if (GetContentClient()->gpu())  // Null in tests.
    GetContentClient()->gpu()->GpuServiceInitialized();

  // Start allowing browser-exposed interfaces to be bound.
  //
  // NOTE: Do not add new binders within this method. Instead modify
  // |ExposeGpuInterfacesToBrowser()| in browser_exposed_gpu_interfaces.cc, as
  // that will ensure security review coverage.
  mojo::BinderMap binders;
  content::ExposeGpuInterfacesToBrowser(
      gpu_service->gpu_preferences(),
      gpu_service->gpu_channel_manager()->gpu_driver_bug_workarounds(),
      &binders);
  ExposeInterfacesToBrowser(std::move(binders));
}

void GpuChildThread::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  auto* gpu_client = GetContentClient()->gpu();
  if (gpu_client)
    gpu_client->PostCompositorThreadCreated(task_runner);
}

void GpuChildThread::QuitMainMessageLoop() {
  quit_closure_.Run();
}

void GpuChildThread::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  if (viz_main_.discardable_shared_memory_manager())
    viz_main_.discardable_shared_memory_manager()->ReleaseFreeMemory();
  SkGraphics::PurgeAllCaches();
}

void GpuChildThread::QuitSafelyHelper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Post a new task (even if we're called on the |task_runner|'s thread) to
  // ensure that we are post-init.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce([]() {
        ChildThreadImpl* current_child_thread = ChildThreadImpl::current();
        if (!current_child_thread)
          return;
        GpuChildThread* gpu_child_thread =
            static_cast<GpuChildThread*>(current_child_thread);
        gpu_child_thread->viz_main_.ExitProcess(
            viz::ExitCode::RESULT_CODE_NORMAL_EXIT);
      }));
}

// Returns a closure which calls into the VizMainImpl to perform shutdown
// before quitting the main message loop. Must be called on the main thread.
base::RepeatingClosure GpuChildThread::MakeQuitSafelyClosure() {
  return base::BindRepeating(&GpuChildThread::QuitSafelyHelper,
                             base::SingleThreadTaskRunner::GetCurrentDefault());
}

#if BUILDFLAG(IS_ANDROID)
// static
std::unique_ptr<media::AndroidOverlay> GpuChildThread::CreateAndroidOverlay(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    const base::UnguessableToken& routing_token,
    media::AndroidOverlayConfig config) {
  mojo::PendingRemote<media::mojom::AndroidOverlayProvider> overlay_provider;
  if (main_task_runner->RunsTasksInCurrentSequence()) {
    ChildThread::Get()->BindHostReceiver(
        overlay_provider.InitWithNewPipeAndPassReceiver());
  } else {
    main_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<media::mojom::AndroidOverlayProvider>
                   receiver) {
              ChildThread::Get()->BindHostReceiver(std::move(receiver));
            },
            overlay_provider.InitWithNewPipeAndPassReceiver()));
  }

  return std::make_unique<media::MojoAndroidOverlay>(
      std::move(overlay_provider), std::move(config), routing_token);
}
#endif

}  // namespace content
