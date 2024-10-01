// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/allocator/partition_alloc_support.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/process/current_process.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/common/skia_utils.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <dwmapi.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/meminfo_dump_provider.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/trace_event/trace_event_etw_export_win.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "media/base/win/mf_initializer.h"
#include "sandbox/policy/win/sandbox_warmup.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/sandboxed_process_thread_type_handler.h"
#include "content/common/gpu_pre_sandbox_hook_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/sandbox_type.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/message_loop/message_pump_apple.h"
#include "components/metal_util/device_removal.h"
#include "gpu/ipc/service/built_in_shader_cache_loader.h"
#include "sandbox/mac/seatbelt.h"
#endif

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool StartSandboxLinux(gpu::GpuWatchdogThread*,
                       const gpu::GPUInfo*,
                       const gpu::GpuPreferences&);
#elif BUILDFLAG(IS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo*);
#endif

class ContentSandboxHelper : public gpu::GpuSandboxHelper {
 public:
  ContentSandboxHelper() {}

  ContentSandboxHelper(const ContentSandboxHelper&) = delete;
  ContentSandboxHelper& operator=(const ContentSandboxHelper&) = delete;

  ~ContentSandboxHelper() override {}

#if BUILDFLAG(IS_WIN)
  void set_sandbox_info(const sandbox::SandboxInterfaceInfo* info) {
    sandbox_info_ = info;
  }
#endif

 private:
  // SandboxHelper:
  void PreSandboxStartup(const gpu::GpuPreferences& gpu_prefs) override {
    // Warm up resources that don't need access to GPUInfo.
    {
      TRACE_EVENT0("gpu", "Warm up rand");
      // Warm up the random subsystem, which needs to be done pre-sandbox on all
      // platforms.
#if BUILDFLAG(IS_WIN)
      sandbox::policy::WarmupRandomnessInfrastructure();
#else
      std::ignore = base::RandUint64();
#endif  // BUILDFLAG(IS_WIN)
    }

#if BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(IS_CHROMEOS)
    media::VaapiWrapper::PreSandboxInitialization();
#else  // For Linux with VA-API support.
    if (!gpu_prefs.disable_accelerated_video_decode)
      media::VaapiWrapper::PreSandboxInitialization();
#endif
#endif  // BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(IS_WIN)
    media::PreSandboxMediaFoundationInitialization();
#endif

    // On Linux, reading system memory doesn't work through the GPU sandbox.
    // This value is cached, so access it here to populate the cache.
    base::SysInfo::AmountOfPhysicalMemory();
  }

  bool EnsureSandboxInitialized(gpu::GpuWatchdogThread* watchdog_thread,
                                const gpu::GPUInfo* gpu_info,
                                const gpu::GpuPreferences& gpu_prefs) override {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    return StartSandboxLinux(watchdog_thread, gpu_info, gpu_prefs);
#elif BUILDFLAG(IS_WIN)
    return StartSandboxWindows(sandbox_info_);
#elif BUILDFLAG(IS_MAC)
    return sandbox::Seatbelt::IsSandboxed();
#else
    return false;
#endif
  }

#if BUILDFLAG(IS_WIN)
  raw_ptr<const sandbox::SandboxInterfaceInfo> sandbox_info_ = nullptr;
#endif
};

void LoadMetalShaderCacheIfNecessary() {
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kUseBuiltInMetalShaderCache)) {
    gpu::BuiltInShaderCacheLoader::StartLoading();
  }
#endif
}

}  // namespace

// Main function for starting the Gpu process.
int GpuMain(MainFunctionParams parameters) {
  TRACE_EVENT0("gpu", "GpuMain");
  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_GPU);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventGpuProcessSortIndex);

  const base::CommandLine& command_line = *parameters.command_line;

  // Start this early on as it reads from a file (in the background) and full
  // startup is gated by this completing.
  LoadMetalShaderCacheIfNecessary();

  gpu::GpuPreferences gpu_preferences;
  if (command_line.HasSwitch(switches::kGpuPreferences)) {
    std::string value =
        command_line.GetSwitchValueASCII(switches::kGpuPreferences);
    bool success = gpu_preferences.FromSwitchValue(value);
    CHECK(success);
  }

  // Disallow sending sync IPCs from the GPU process, in particular CrGpuMain
  // and VizCompositorThreads. Incoming sync IPCs can be received out of order
  // when waiting on response to an outgoing sync IPC. Both viz and gpu
  // interfaces rely on receiving messages in order so this message reordering
  // would break things.
  mojo::SyncCallRestrictions::DisallowSyncCall();

  if (gpu_preferences.gpu_startup_dialog)
    WaitForDebugger("Gpu");

  base::TimeTicks start_time = base::TimeTicks::Now();

#if BUILDFLAG(IS_WIN)
  base::win::EnableHighDPISupport();

  // Prevent Windows from displaying a modal dialog on failures like not being
  // able to load a DLL.
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);

  // COM is used by some Windows Media Foundation calls made on this thread and
  // must be MTA so we don't have to worry about pumping messages to handle
  // COM callbacks.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);

  // A higher priority class is used for the GPU process so that it remains at
  // a higher priority than renderer processes.
  ::SetPriorityClass(::GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
#endif

  // Installs a base::LogMessageHandlerFunction which ensures messages are sent
  // to the GpuProcessHost once the GpuServiceImpl has started.
  viz::GpuServiceImpl::InstallPreInitializeLogHandler();

  // We are experiencing what appear to be memory-stomp issues in the GPU
  // process. These issues seem to be impacting the task executor and listeners
  // registered to it. Create the task executor on the heap to guard against
  // this.
  // TODO(ericrk): Revisit this once we assess its impact on crbug.com/662802
  // and crbug.com/609252.
  std::unique_ptr<base::SingleThreadTaskExecutor> main_thread_task_executor;
  std::unique_ptr<ui::PlatformEventSource> event_source;
  if (command_line.HasSwitch(switches::kHeadless)) {
#if BUILDFLAG(IS_MAC)
    // CADisplayLink (Mac HW VSync) callback only works with NS_RUNLOOP.
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::NS_RUNLOOP);
    main_thread_task_executor->SetWorkBatchSize(2);
#else
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::DEFAULT);
#endif
  } else {
#if BUILDFLAG(IS_WIN)
    // The GpuMain thread should not be pumping Windows messages because no UI
    // is expected to run on this thread.
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::DEFAULT);
#elif BUILDFLAG(IS_OZONE)
    // The MessagePump type required depends on the Ozone platform selected at
    // runtime.
    if (!main_thread_task_executor) {
      main_thread_task_executor =
          std::make_unique<base::SingleThreadTaskExecutor>(
              gpu_preferences.message_pump_type);
    }
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#error "Unsupported Linux platform."
#elif BUILDFLAG(IS_MAC)
    // Cross-process CoreAnimation requires a CFRunLoop to function at all, and
    // requires a NSRunLoop to not starve under heavy load. See:
    // https://crbug.com/312462#c51 and https://crbug.com/783298
    // CADisplayLink (Mac HW VSync) callback only works with NS_RUNLOOP. DEFAULT
    // type does not support NSObject.
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::NS_RUNLOOP);
    // As part of the migration to DoWork(), this policy is required to keep
    // previous behavior and avoid regressions.
    // TODO(crbug.com/40668161): Consider updating the policy.
    main_thread_task_executor->SetWorkBatchSize(2);
#else
    main_thread_task_executor =
        std::make_unique<base::SingleThreadTaskExecutor>(
            base::MessagePumpType::DEFAULT);
#endif
  }

  base::PlatformThread::SetName("CrGpuMain");
  mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics("GpuMain");

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Thread type delegate of the process should be registered before
  // thread type change below for the main thread and for thread pool in
  // ChildProcess constructor.
  // It also needs to be registered before the process has multiple threads,
  // which may race with application of the sandbox. InitializeAndStartSandbox()
  // sandboxes the process and starts threads so this has to happen first.
  if (base::FeatureList::IsEnabled(
          features::kHandleChildThreadTypeChangesInBrowser) ||
      base::FeatureList::IsEnabled(features::kSchedQoSOnResourcedForChrome)) {
    SandboxedProcessThreadTypeHandler::Create();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  base::PlatformThread::SetCurrentThreadType(
      base::ThreadType::kDisplayCritical);

  auto gpu_init = std::make_unique<gpu::GpuInit>();
  ContentSandboxHelper sandbox_helper;
#if BUILDFLAG(IS_WIN)
  sandbox_helper.set_sandbox_info(parameters.sandbox_info);
#endif

  gpu_init->set_sandbox_helper(&sandbox_helper);

  // Since GPU initialization calls into skia, it's important to initialize skia
  // before it.
  InitializeSkia();

  // The ThreadPool must have been created before invoking |gpu_init| as it
  // needs the ThreadPool (in angle::InitializePlatform()). Do not start it
  // until after the sandbox is initialized however to avoid creating threads
  // outside the sandbox.
  DCHECK(base::ThreadPoolInstance::Get());

  // Gpu initialization may fail for various reasons, in which case we will need
  // to tear down this process. However, we can not do so safely until the IPC
  // channel is set up, because the detection of early return of a child process
  // is implemented using an IPC channel error. If the IPC channel is not fully
  // set up between the browser and GPU process, and the GPU process crashes or
  // exits early, the browser process will never detect it.  For this reason we
  // defer tearing down the GPU process until receiving the initialization
  // message from the browser (through mojom::VizMain::CreateGpuService()).
  const bool init_success = gpu_init->InitializeAndStartSandbox(
      const_cast<base::CommandLine*>(&command_line), gpu_preferences);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  LOG(WARNING) << "gpu initialization completed init_success:" << init_success;
#endif
  const bool dead_on_arrival = !init_success;

  auto* client = GetContentClient()->gpu();
  if (client) {
    client->PostSandboxInitialized();
  }

  GetContentClient()->SetGpuInfo(gpu_init->gpu_info());

  base::ThreadType io_thread_type = base::ThreadType::kDisplayCritical;
  // ChildProcess will start the ThreadPoolInstance now that the sandbox is
  // initialized.
  ChildProcess gpu_process(io_thread_type);
  DCHECK(base::ThreadPoolInstance::Get()->WasStarted());

  if (client) {
    client->PostIOThreadCreated(gpu_process.io_task_runner());
  }

  base::RunLoop run_loop;
  GpuChildThread* child_thread =
      new GpuChildThread(run_loop.QuitClosure(), std::move(gpu_init));
  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  // Mojo IPC support is brought up by GpuChildThread, so startup tracing is
  // enabled here if it needs to start after mojo init (normally so the mojo
  // broker can bypass the sandbox to allocate startup tracing's SMB).
  if (parameters.needs_startup_tracing_after_mojo_init) {
    tracing::EnableStartupTracingIfNeeded();
  }

#if BUILDFLAG(IS_MAC)
  // A GPUEjectPolicy of 'wait' is set in the Info.plist of the browser
  // process, meaning it is "responsible" for making sure it and its
  // subordinate processes (i.e. the GPU process) drop references to the
  // external GPU. Despite this, the system still sends the device removal
  // notifications to the GPU process, so the GPU process handles its own
  // graceful shutdown without help from the browser process.
  //
  // Using the "SafeEjectGPU" tool, we can see that when the browser process
  // has a policy of 'wait', the GPU process gets the 'rwait' policy: "Eject
  // actions apply to the responsible process, who in turn deals with
  // subordinates to eliminate their ejecting eGPU references" [man 8
  // SafeEjectGPU]. Empirically, the browser does not relaunch. Once the GPU
  // process exits, it appears that the browser process is no longer considered
  // to be using the GPU, so it "succeeds" the 'wait'.
  metal::RegisterGracefulExitOnDeviceRemoval();
#endif

#if BUILDFLAG(IS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);

  base::android::MeminfoDumpProvider::Initialize();
#endif

  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      switches::kGpuProcess);

  base::HighResolutionTimerManager hi_res_timer_manager;

  // Adds support of wall-time based TimerKeeper metrics for the main GPU thread
  // when command-line flag is set. CrGpuMain will be used as suffix for each
  // metric.
  if (command_line.HasSwitch(switches::kEnableGpuMainTimeKeeperMetrics)) {
    base::CurrentThread::Get()->EnableMessagePumpTimeKeeperMetrics(
        "CrGpuMain",
        /*wall_time_based_metrics_enabled_for_testing=*/true);
  }

  {
    TRACE_EVENT0("gpu", "Run Message Loop");
    run_loop.Run();
  }

  return dead_on_arrival ? RESULT_CODE_GPU_DEAD_ON_ARRIVAL : 0;
}

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool StartSandboxLinux(gpu::GpuWatchdogThread* watchdog_thread,
                       const gpu::GPUInfo* gpu_info,
                       const gpu::GpuPreferences& gpu_prefs) {
  TRACE_EVENT0("gpu,startup", "Initialize sandbox");

  if (watchdog_thread) {
    // SandboxLinux needs to be able to ensure that the thread
    // has really been stopped.
    sandbox::policy::SandboxLinux::GetInstance()->StopThread(watchdog_thread);
  }

  // SandboxLinux::InitializeSandbox() must always be called
  // with only one thread.
  sandbox::policy::SandboxLinux::Options sandbox_options;
  if (gpu_info) {
    // We have to enable sandbox settings for all GPUs in the system
    // for Chrome to be able to access/use them.
    sandbox_options.use_amd_specific_policies =
        angle::IsAMD(gpu_info->active_gpu().vendor_id);
    sandbox_options.use_intel_specific_policies =
        angle::IsIntel(gpu_info->active_gpu().vendor_id);
    sandbox_options.use_virtio_specific_policies =
        angle::IsVirtIO(gpu_info->active_gpu().vendor_id);
    sandbox_options.use_nvidia_specific_policies =
        angle::IsNVIDIA(gpu_info->active_gpu().vendor_id);
    for (const auto& gpu : gpu_info->secondary_gpus) {
      if (angle::IsAMD(gpu.vendor_id))
        sandbox_options.use_amd_specific_policies = true;
      else if (angle::IsIntel(gpu.vendor_id))
        sandbox_options.use_intel_specific_policies = true;
      else if (angle::IsNVIDIA(gpu.vendor_id))
        sandbox_options.use_nvidia_specific_policies = true;
    }
  }
  sandbox_options.accelerated_video_decode_enabled =
      !gpu_prefs.disable_accelerated_video_decode;
  sandbox_options.accelerated_video_encode_enabled =
      !gpu_prefs.disable_accelerated_video_encode;

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Video decoding of many video streams can use thousands of FDs as well as
  // Exo clients like Lacros.
  // See https://crbug.com/1417237
  const auto current_max_fds =
      base::saturated_cast<unsigned int>(base::GetMaxFds());
  constexpr unsigned int kMaxFDsDelta = 1u << 13;
  const auto new_max_fds =
      static_cast<unsigned int>(base::ClampMax(current_max_fds, kMaxFDsDelta));
  base::IncreaseFdLimitTo(new_max_fds);
#endif

  bool res = sandbox::policy::SandboxLinux::GetInstance()->InitializeSandbox(
      sandbox::policy::SandboxTypeFromCommandLine(
          *base::CommandLine::ForCurrentProcess()),
      base::BindOnce(GpuPreSandboxHook), sandbox_options);

  if (watchdog_thread) {
    watchdog_thread->Start();
  }

  return res;
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo* sandbox_info) {
  TRACE_EVENT0("gpu,startup", "Lower token");

  // For Windows, if the target_services interface is not zero, the process
  // is sandboxed and we must call LowerToken() before rendering untrusted
  // content.
  sandbox::TargetServices* target_services = sandbox_info->target_services;
  if (target_services) {
    target_services->LowerToken();
    return true;
  }

  return false;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace.

}  // namespace content
