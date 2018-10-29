// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_sampler_profiler.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/common/skia_utils.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/gpu_init.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "media/gpu/buildflags.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include <windows.h>
#include <dwmapi.h>
#endif

#if defined(OS_ANDROID)
#include "base/trace_event/memory_dump_manager.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#endif

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_version.h"
#include "media/gpu/windows/dxva_video_decode_accelerator_win.h"
#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"       // nogncheck
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#include "ui/gfx/x/x11_switches.h"    // nogncheck
#endif

#if defined(OS_LINUX)
#include "content/gpu/gpu_sandbox_hook_linux.h"
#include "content/public/common/sandbox_init.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"
#include "services/service_manager/zygote/common/common_sandbox_support_linux.h"
#endif

#if defined(OS_MACOSX)
#include "base/message_loop/message_pump_mac.h"
#include "sandbox/mac/seatbelt.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if defined(OS_MACOSX)
extern "C" {
void _LSSetApplicationLaunchServicesServerConnectionStatus(
    uint64_t flags,
    bool (^connection_allowed)(CFDictionaryRef));
};
#endif  // defined(OS_MACOSX)

namespace content {

namespace {

#if defined(OS_LINUX)
bool StartSandboxLinux(gpu::GpuWatchdogThread*,
                       const gpu::GPUInfo*,
                       const gpu::GpuPreferences&);
#elif defined(OS_WIN)
bool StartSandboxWindows(const sandbox::SandboxInterfaceInfo*);
#endif

base::LazyInstance<viz::VizMainImpl::LogMessages>::DestructorAtExit
    deferred_messages = LAZY_INSTANCE_INITIALIZER;

bool GpuProcessLogMessageHandler(int severity,
                                 const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  viz::VizMainImpl::LogMessage log;
  log.severity = severity;
  log.header = str.substr(0, message_start);
  log.message = str.substr(message_start);
  deferred_messages.Get().push_back(std::move(log));
  return false;
}

class ContentSandboxHelper : public gpu::GpuSandboxHelper {
 public:
  ContentSandboxHelper() {}
  ~ContentSandboxHelper() override {}

#if defined(OS_WIN)
  void set_sandbox_info(const sandbox::SandboxInterfaceInfo* info) {
    sandbox_info_ = info;
  }
#endif

 private:
  // SandboxHelper:
  void PreSandboxStartup() override {
    // Warm up resources that don't need access to GPUInfo.
    {
      TRACE_EVENT0("gpu", "Warm up rand");
      // Warm up the random subsystem, which needs to be done pre-sandbox on all
      // platforms.
      (void)base::RandUint64();
    }

#if BUILDFLAG(USE_VAAPI)
    media::VaapiWrapper::PreSandboxInitialization();
#endif
#if defined(OS_WIN)
    media::DXVAVideoDecodeAccelerator::PreSandboxInitialization();
    media::MediaFoundationVideoEncodeAccelerator::PreSandboxInitialization();
#endif

    // On Linux, reading system memory doesn't work through the GPU sandbox.
    // This value is cached, so access it here to populate the cache.
    base::SysInfo::AmountOfPhysicalMemory();
  }

  bool EnsureSandboxInitialized(gpu::GpuWatchdogThread* watchdog_thread,
                                const gpu::GPUInfo* gpu_info,
                                const gpu::GpuPreferences& gpu_prefs) override {
#if defined(OS_LINUX)
    return StartSandboxLinux(watchdog_thread, gpu_info, gpu_prefs);
#elif defined(OS_WIN)
    return StartSandboxWindows(sandbox_info_);
#elif defined(OS_MACOSX)
    return sandbox::Seatbelt::IsSandboxed();
#else
    return false;
#endif
  }

#if defined(OS_WIN)
  const sandbox::SandboxInterfaceInfo* sandbox_info_ = nullptr;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentSandboxHelper);
};

}  // namespace

// Main function for starting the Gpu process.
int GpuMain(const MainFunctionParams& parameters) {
  TRACE_EVENT0("gpu", "GpuMain");
  base::trace_event::TraceLog::GetInstance()->set_process_name("GPU Process");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventGpuProcessSortIndex);

  const base::CommandLine& command_line = parameters.command_line;

  gpu::GpuPreferences gpu_preferences;
  if (command_line.HasSwitch(switches::kGpuPreferences)) {
    std::string value =
        command_line.GetSwitchValueASCII(switches::kGpuPreferences);
    bool success = gpu_preferences.FromSwitchValue(value);
    CHECK(success);
  }

  if (gpu_preferences.gpu_startup_dialog)
    WaitForDebugger("Gpu");

  base::Time start_time = base::Time::Now();

#if defined(OS_WIN)
  // Prevent Windows from displaying a modal dialog on failures like not being
  // able to load a DLL.
  SetErrorMode(
      SEM_FAILCRITICALERRORS |
      SEM_NOGPFAULTERRORBOX |
      SEM_NOOPENFILEERRORBOX);

  // COM is used by some Windows Media Foundation calls made on this thread and
  // must be MTA so we don't have to worry about pumping messages to handle
  // COM callbacks.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
#endif

  logging::SetLogMessageHandler(GpuProcessLogMessageHandler);

  // We are experiencing what appear to be memory-stomp issues in the GPU
  // process. These issues seem to be impacting the message loop and listeners
  // registered to it. Create the message loop on the heap to guard against
  // this.
  // TODO(ericrk): Revisit this once we assess its impact on crbug.com/662802
  // and crbug.com/609252.
  std::unique_ptr<base::MessageLoop> main_message_loop;
  std::unique_ptr<ui::PlatformEventSource> event_source;
  if (command_line.HasSwitch(switches::kHeadless)) {
    main_message_loop.reset(
        new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
  } else {
#if defined(OS_WIN)
    // The GpuMain thread should not be pumping Windows messages because no UI
    // is expected to run on this thread.
    main_message_loop.reset(
        new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
#elif defined(USE_X11)
    // Depending on how Chrome is running there are multiple threads that can
    // make Xlib function calls. Call XInitThreads() here to be safe, even if
    // some configurations don't strictly need it.
    gfx::InitializeThreadedX11();

    // We need a UI loop so that we can grab the Expose events. See GLSurfaceGLX
    // and https://crbug.com/326995.
    ui::SetDefaultX11ErrorHandlers();
    if (!gfx::GetXDisplay())
      return RESULT_CODE_GPU_DEAD_ON_ARRIVAL;
    main_message_loop.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
    event_source = ui::PlatformEventSource::CreateDefault();
#elif defined(USE_OZONE)
    // The MessageLoop type required depends on the Ozone platform selected at
    // runtime.
    main_message_loop.reset(new base::MessageLoop(
        ui::OzonePlatform::EnsureInstance()->GetMessageLoopTypeForGpu()));
#elif defined(OS_LINUX)
#error "Unsupported Linux platform."
#elif defined(OS_MACOSX)
    // Cross-process CoreAnimation requires a CFRunLoop to function at all, and
    // requires a NSRunLoop to not starve under heavy load. See:
    // https://crbug.com/312462#c51 and https://crbug.com/783298
    std::unique_ptr<base::MessagePump> pump(new base::MessagePumpNSRunLoop());
    main_message_loop.reset(new base::MessageLoop(std::move(pump)));

    // Tell LaunchServices to continue without a connection to the daemon.
    _LSSetApplicationLaunchServicesServerConnectionStatus(0, nullptr);
#else
    main_message_loop.reset(
        new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
#endif
  }

  base::PlatformThread::SetName("CrGpuMain");

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Set thread priority before sandbox initialization.
  base::PlatformThread::SetCurrentThreadPriority(base::ThreadPriority::DISPLAY);
#endif

  auto gpu_init = std::make_unique<gpu::GpuInit>();
  ContentSandboxHelper sandbox_helper;
#if defined(OS_WIN)
  sandbox_helper.set_sandbox_info(parameters.sandbox_info);
#endif

  gpu_init->set_sandbox_helper(&sandbox_helper);

  // Since GPU initialization calls into skia, its important to initialize skia
  // before it.
  InitializeSkia();

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
  const bool dead_on_arrival = !init_success;

  logging::SetLogMessageHandler(nullptr);
  GetContentClient()->SetGpuInfo(gpu_init->gpu_info());

  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  io_thread_priority = base::ThreadPriority::DISPLAY;
#endif

  GpuProcess gpu_process(io_thread_priority);

  auto* client = GetContentClient()->gpu();
  if (client)
    client->PostIOThreadCreated(gpu_process.io_task_runner());

  base::RunLoop run_loop;
  GpuChildThread* child_thread =
      new GpuChildThread(run_loop.QuitClosure(), std::move(gpu_init),
                         std::move(deferred_messages.Get()));
  deferred_messages.Get().clear();

  child_thread->Init(start_time);

  gpu_process.set_main_thread(child_thread);

  // Setup tracing sampler profiler as early as possible.
  auto tracing_sampler_profiler =
      std::make_unique<tracing::TracingSamplerProfiler>(
          base::PlatformThread::CurrentId());
  tracing_sampler_profiler->OnMessageLoopStarted();

#if defined(OS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);
#endif

  base::HighResolutionTimerManager hi_res_timer_manager;

  {
    TRACE_EVENT0("gpu", "Run Message Loop");
    run_loop.Run();
  }

  return dead_on_arrival ? RESULT_CODE_GPU_DEAD_ON_ARRIVAL : 0;
}

namespace {

#if defined(OS_LINUX)
bool StartSandboxLinux(gpu::GpuWatchdogThread* watchdog_thread,
                       const gpu::GPUInfo* gpu_info,
                       const gpu::GpuPreferences& gpu_prefs) {
  TRACE_EVENT0("gpu,startup", "Initialize sandbox");

  if (watchdog_thread) {
    // SandboxLinux needs to be able to ensure that the thread
    // has really been stopped.
    service_manager::SandboxLinux::GetInstance()->StopThread(watchdog_thread);
  }

  // SandboxLinux::InitializeSandbox() must always be called
  // with only one thread.
  service_manager::SandboxLinux::Options sandbox_options;
  sandbox_options.use_amd_specific_policies =
      gpu_info && angle::IsAMD(gpu_info->active_gpu().vendor_id);
  sandbox_options.accelerated_video_decode_enabled =
      !gpu_prefs.disable_accelerated_video_decode;
  sandbox_options.accelerated_video_encode_enabled =
      !gpu_prefs.disable_accelerated_video_encode;

  bool res = service_manager::SandboxLinux::GetInstance()->InitializeSandbox(
      service_manager::SandboxTypeFromCommandLine(
          *base::CommandLine::ForCurrentProcess()),
      base::BindOnce(GpuProcessPreSandboxHook), sandbox_options);

  if (watchdog_thread) {
    base::Thread::Options thread_options;
    thread_options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    watchdog_thread->StartWithOptions(thread_options);
  }

  return res;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
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
#endif  // defined(OS_WIN)

}  // namespace.

}  // namespace content
