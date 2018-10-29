// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/pending_task.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/process_metrics.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/system_monitor.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/initialization_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/tracing/common/trace_startup_config.h"
#include "components/tracing/common/trace_to_console.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/compositing_mode_reporter_impl.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/compositor/viz_process_transport_factory.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/storage_area_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/histogram_synchronizer.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/memory/swap_metrics_delegate_uma.h"
#include "content/browser/net/browser_online_state_observer.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/scheduler/responsiveness/watcher.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_task_runner.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/browser/webui/content_web_ui_controller_factory.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/service_manager/service_manager_connection_impl.h"
#include "content/common/task_scheduler.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/swap_metrics_driver.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/service_names.mojom.h"
#include "device/gamepad/gamepad_service.h"
#include "gpu/config/gpu_switches.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system.h"
#include "media/audio/audio_thread_impl.h"
#include "media/base/media.h"
#include "media/base/user_input_monitor.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_service.h"
#include "media/mojo/buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/network_change_notifier.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/audio/public/cpp/audio_system_factory.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/content/public/cpp/navigable_contents_view.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/client_process_impl.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/zygote/common/zygote_buildflags.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "sql/sql_memory_dump_provider.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/switches.h"

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/public/browser/context_factory.h"
#include "ui/aura/env.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/trace_event/cpufreq_monitor_android.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/launcher_thread.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"
#include "ui/android/screen_android.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_surface.h"
#endif

#if defined(OS_MACOSX)
#include "base/memory/memory_pressure_monitor_mac.h"
#include "content/browser/cocoa/system_hotkey_helper_mac.h"
#include "content/browser/mach_broker_mac.h"
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/theme_helper_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include "base/memory/memory_pressure_monitor_win.h"
#include "net/base/winsock_init.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/display/win/screen_win.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/memory/memory_pressure_monitor_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

#if defined(USE_GLIB)
#include <glib-object.h>
#endif

#if defined(OS_WIN)
#include "media/device_monitors/system_message_window_win.h"
#elif defined(OS_LINUX) && defined(USE_UDEV)
#include "media/device_monitors/device_monitor_udev.h"
#elif defined(OS_MACOSX)
#include "media/device_monitors/device_monitor_mac.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/job.h>

#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif  // defined(OS_FUCHSIA)

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "content/browser/sandbox_host_linux.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/cdm_registry_impl.h"
#endif

#if defined(USE_X11)
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "ui/base/x/x11_util_internal.h"  // nogncheck
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#include "ui/gfx/x/x11_types.h"  // nogncheck
#endif

#if defined(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#endif

#if defined(ENABLE_IPC_FUZZER) && defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

namespace content {
namespace {

#if defined(USE_GLIB)
static void GLibLogHandler(const gchar* log_domain,
                           GLogLevelFlags log_level,
                           const gchar* message,
                           gpointer userdata) {
  if (!log_domain)
    log_domain = "<unknown>";
  if (!message)
    message = "<no message>";

  GLogLevelFlags always_fatal_flags = g_log_set_always_fatal(G_LOG_LEVEL_MASK);
  g_log_set_always_fatal(always_fatal_flags);
  GLogLevelFlags fatal_flags =
      g_log_set_fatal_mask(log_domain, G_LOG_LEVEL_MASK);
  g_log_set_fatal_mask(log_domain, fatal_flags);
  if ((always_fatal_flags | fatal_flags) & log_level) {
    LOG(DFATAL) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL)) {
    LOG(ERROR) << log_domain << ": " << message;
  } else if (log_level & (G_LOG_LEVEL_WARNING)) {
    LOG(WARNING) << log_domain << ": " << message;
  } else if (log_level &
             (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)) {
    LOG(INFO) << log_domain << ": " << message;
  } else {
    NOTREACHED();
    LOG(DFATAL) << log_domain << ": " << message;
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* const kLogDomains[] =
      { nullptr, "Gtk", "Gdk", "GLib", "GLib-GObject" };
  for (size_t i = 0; i < arraysize(kLogDomains); i++) {
    g_log_set_handler(
        kLogDomains[i],
        static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL |
                                    G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                                    G_LOG_LEVEL_WARNING),
        GLibLogHandler, nullptr);
  }
}
#endif  // defined(USE_GLIB)

void OnStoppedStartupTracing(const base::FilePath& trace_file) {
  VLOG(0) << "Completed startup tracing to " << trace_file.value();
}

// Tell compiler not to inline this function so it's possible to tell what
// thread was unresponsive by inspecting the callstack.
NOINLINE void ResetThread_IO(
    std::unique_ptr<BrowserProcessSubThread> io_thread) {
  const int line_number = __LINE__;
  io_thread.reset();
  base::debug::Alias(&line_number);
}

#if defined(OS_WIN)
// Creates a memory pressure monitor using automatic thresholds, or those
// specified on the command-line. Ownership is passed to the caller.
std::unique_ptr<base::win::MemoryPressureMonitor>
CreateWinMemoryPressureMonitor(const base::CommandLine& parsed_command_line) {
  std::vector<std::string> thresholds =
      base::SplitString(parsed_command_line.GetSwitchValueASCII(
                            switches::kMemoryPressureThresholdsMb),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  int moderate_threshold_mb = 0;
  int critical_threshold_mb = 0;
  if (thresholds.size() == 2 &&
      base::StringToInt(thresholds[0], &moderate_threshold_mb) &&
      base::StringToInt(thresholds[1], &critical_threshold_mb) &&
      moderate_threshold_mb >= critical_threshold_mb &&
      critical_threshold_mb >= 0) {
    return std::make_unique<base::win::MemoryPressureMonitor>(
        moderate_threshold_mb, critical_threshold_mb);
  }

  // In absence of valid switches use the automatic defaults.
  return std::make_unique<base::win::MemoryPressureMonitor>();
}
#endif  // defined(OS_WIN)

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_BLOCKING,
  FOREGROUND,
  FOREGROUND_BLOCKING,
  WORKER_POOL_COUNT  // Always last.
};

#if !defined(OS_FUCHSIA)
// Time between updating and recording swap rates.
constexpr base::TimeDelta kSwapMetricsInterval =
    base::TimeDelta::FromSeconds(60);
#endif  // !defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
// Create and register the job which will contain all child processes
// of the browser process as well as their descendents.
void InitDefaultJob() {
  zx::job job;
  zx_status_t result = zx::job::create(*zx::job::default_job(), 0, &job);
  ZX_CHECK(ZX_OK == result, result) << "zx_job_create";
  base::SetDefaultJob(std::move(job));
}
#endif  // defined(OS_FUCHSIA)

#if defined(ENABLE_IPC_FUZZER)
bool GetBuildDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_EXE, result))
    return false;

#if defined(OS_MACOSX)
  if (base::mac::AmIBundled()) {
    // The bundled app executables (Chromium, TestShell, etc) live three
    // levels down from the build directory, eg:
    // Chromium.app/Contents/MacOS/Chromium
    *result = result->DirName().DirName().DirName();
  }
#endif
  return true;
}

void SetFileUrlPathAliasForIpcFuzzer() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kFileUrlPathAlias))
    return;

  base::FilePath build_directory;
  if (!GetBuildDirectory(&build_directory)) {
    LOG(ERROR) << "Failed to get build directory for /gen path alias.";
    return;
  }

  const base::CommandLine::StringType alias_switch =
      FILE_PATH_LITERAL("/gen=") + build_directory.AppendASCII("gen").value();
  base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
      switches::kFileUrlPathAlias, alias_switch);
}
#endif

std::unique_ptr<base::MemoryPressureMonitor> CreateMemoryPressureMonitor(
    const base::CommandLine& command_line) {
  // Behavior of browser tests should not depend on things outside of their
  // control (like the amount of memory on the system running the tests).
  if (command_line.HasSwitch(switches::kBrowserTest))
    return nullptr;

// TODO(chrisha): Simplify this code once MemoryPressureMonitor is made a
// concrete class.
#if defined(OS_CHROMEOS)
  if (chromeos::switches::MemoryPressureHandlingEnabled()) {
    return std::make_unique<base::chromeos::MemoryPressureMonitor>(
        chromeos::switches::GetMemoryPressureThresholds());
  }
  return nullptr;
#elif defined(OS_MACOSX)
  return std::make_unique<base::mac::MemoryPressureMonitor>();
#elif defined(OS_WIN)
  return CreateWinMemoryPressureMonitor(command_line);
#else
  // No memory monitor on other platforms...
  return nullptr;
#endif
}

}  // namespace

#if defined(USE_X11)
namespace internal {

// Forwards GPUInfo updates to ui::XVisualManager
class GpuDataManagerVisualProxy : public GpuDataManagerObserver {
 public:
  explicit GpuDataManagerVisualProxy(GpuDataManagerImpl* gpu_data_manager)
      : gpu_data_manager_(gpu_data_manager) {
    gpu_data_manager_->AddObserver(this);
  }

  ~GpuDataManagerVisualProxy() override {
    gpu_data_manager_->RemoveObserver(this);
  }

  void OnGpuInfoUpdate() override {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kHeadless))
      return;
    gpu::GPUInfo gpu_info = gpu_data_manager_->GetGPUInfo();
    if (!ui::XVisualManager::GetInstance()->OnGPUInfoChanged(
            gpu_info.software_rendering ||
                !gpu_data_manager_->GpuAccessAllowed(nullptr),
            gpu_info.system_visual, gpu_info.rgba_visual)) {
      // The GPU process sent back bad visuals, which should never happen.
      auto* gpu_process_host = GpuProcessHost::Get(
          GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED, false);
      if (gpu_process_host)
        gpu_process_host->ForceShutdown();
    }
  }

 private:
  GpuDataManagerImpl* gpu_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerVisualProxy);
};

}  // namespace internal
#endif

#if defined(OS_WIN)
namespace {

// Provides a bridge whereby display::win::ScreenWin can ask the GPU process
// about the HDR status of the system.
class HDRProxy {
 public:
  static void Initialize() {
    display::win::ScreenWin::SetRequestHDRStatusCallback(
        base::BindRepeating(&HDRProxy::RequestHDRStatus));
  }

  static void RequestHDRStatus() {
    // The request must be sent to the GPU process from the IO thread.
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(&HDRProxy::RequestOnIOThread));
  }

 private:
  static void RequestOnIOThread() {
    auto* gpu_process_host =
        GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED, false);
    if (gpu_process_host) {
      auto* gpu_service = gpu_process_host->gpu_host()->gpu_service();
      gpu_service->RequestHDRStatus(
          base::BindOnce(&HDRProxy::GotResultOnIOThread));
    } else {
      bool hdr_enabled = false;
      GotResultOnIOThread(hdr_enabled);
    }
  }
  static void GotResultOnIOThread(bool hdr_enabled) {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(&HDRProxy::GotResult, hdr_enabled));
  }
  static void GotResult(bool hdr_enabled) {
    display::win::ScreenWin::SetHDREnabled(hdr_enabled);
  }
};

}  // namespace
#endif

// The currently-running BrowserMainLoop.  There can be one or zero.
BrowserMainLoop* g_current_browser_main_loop = nullptr;

#if defined(OS_ANDROID)
bool g_browser_main_loop_shutting_down = false;
#endif

// BrowserMainLoop construction / destruction =============================

BrowserMainLoop* BrowserMainLoop::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_current_browser_main_loop;
}

// static
media::AudioManager* BrowserMainLoop::GetAudioManager() {
  return g_current_browser_main_loop->audio_manager();
}

BrowserMainLoop::BrowserMainLoop(
    const MainFunctionParams& parameters,
    std::unique_ptr<base::TaskScheduler::ScopedExecutionFence>
        scoped_execution_fence)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(service_manager::RESULT_CODE_NORMAL_EXIT),
      created_threads_(false),
      scoped_execution_fence_(std::move(scoped_execution_fence)) {
  DCHECK(!g_current_browser_main_loop);
  DCHECK(scoped_execution_fence_)
      << "TaskScheduler must be halted before kicking off content.";
  g_current_browser_main_loop = this;

  if (GetContentClient()->browser()->ShouldCreateTaskScheduler()) {
    DCHECK(base::TaskScheduler::GetInstance());
  }
}

BrowserMainLoop::~BrowserMainLoop() {
  DCHECK_EQ(this, g_current_browser_main_loop);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  g_current_browser_main_loop = nullptr;
}

void BrowserMainLoop::Init() {
  TRACE_EVENT0("startup", "BrowserMainLoop::Init");

  // |startup_data| is optional. If set, the thread owned by the data
  // will be registered as BrowserThread::IO in CreateThreads() instead of
  // creating a brand new thread.
  if (parameters_.startup_data) {
    StartupDataImpl* startup_data =
        static_cast<StartupDataImpl*>(parameters_.startup_data);
    // This is always invoked before |io_thread_| is initialized (i.e. never
    // resets it).
    io_thread_ = std::move(startup_data->thread);
  }

  parts_.reset(
      GetContentClient()->browser()->CreateBrowserMainParts(parameters_));
}

// BrowserMainLoop stages ==================================================

int BrowserMainLoop::EarlyInitialization() {
  TRACE_EVENT0("startup", "BrowserMainLoop::EarlyInitialization");

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // The initialization of the sandbox host ends up with forking the Zygote
  // process and requires no thread been forked. The initialization has happened
  // by now since a thread to start the ServiceManager has been created
  // before the browser main loop starts.
  DCHECK(SandboxHostLinux::GetInstance()->IsInitialized());
#endif

#if defined(USE_X11)
  if (UsingInProcessGpu()) {
    if (!gfx::InitializeThreadedX11()) {
      LOG(ERROR) << "Failed to put Xlib into threaded mode.";
    }
  }
#endif

  // GLib's spawning of new processes is buggy, so it's important that at this
  // point GLib does not need to start DBUS. Chrome should always start with
  // DBUS_SESSION_BUS_ADDRESS properly set. See crbug.com/309093.
#if defined(USE_GLIB)
  // g_type_init will be deprecated in 2.36. 2.35 is the development
  // version for 2.36, hence do not call g_type_init starting 2.35.
  // http://developer.gnome.org/gobject/unstable/gobject-Type-Information.html#g-type-init
#if !GLIB_CHECK_VERSION(2, 35, 0)
  // GLib type system initialization. It's unclear if it's still required for
  // any remaining code. Most likely this is superfluous as gtk_init() ought
  // to do this. It's definitely harmless, so it's retained here.
  g_type_init();
#endif  // !GLIB_CHECK_VERSION(2, 35, 0)

  SetUpGLibLogHandler();
#endif  // defined(USE_GLIB)

  if (parts_) {
    const int pre_early_init_error_code = parts_->PreEarlyInitialization();
    if (pre_early_init_error_code != service_manager::RESULT_CODE_NORMAL_EXIT)
      return pre_early_init_error_code;
  }

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // Up the priority of the UI thread unless it was already high (since recent
  // versions of Android (O+) do this automatically).
  if (base::PlatformThread::GetCurrentThreadPriority() <
      base::ThreadPriority::DISPLAY) {
    base::PlatformThread::SetCurrentThreadPriority(
        base::ThreadPriority::DISPLAY);
  }
#endif  // defined(OS_ANDROID) || defined(OS_CHROMEOS)

#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_ANDROID)
  // We use quite a few file descriptors for our IPC as well as disk the disk
  // cache,and the default limit on the Mac is low (256), so bump it up.

  // Same for Linux. The default various per distro, but it is 1024 on Fedora.
  // Low soft limits combined with liberal use of file descriptors means power
  // users can easily hit this limit with many open tabs. Bump up the limit to
  // an arbitrarily high number. See https://crbug.com/539567
  base::IncreaseFdLimitTo(8192);
#endif  // defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_CHROMEOS) ||
        // defined(OS_ANDROID)

#if defined(OS_WIN)
  net::EnsureWinsockInit();
#endif

#if defined(USE_NSS_CERTS)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

#if defined(OS_FUCHSIA)
  InitDefaultJob();
#endif

  if (parsed_command_line_.HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit_string = parsed_command_line_.GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    size_t process_limit;
    if (base::StringToSizeT(limit_string, &process_limit)) {
      RenderProcessHost::SetMaxRendererProcessCount(process_limit);
    }
  }

  if (parts_)
    parts_->PostEarlyInitialization();

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainLoop::PreMainMessageLoopStart() {
  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::MainMessageLoopStart:PreMainMessageLoopStart");
    parts_->PreMainMessageLoopStart();
  }
}

void BrowserMainLoop::MainMessageLoopStart() {
  // DO NOT add more code here. Use PreMainMessageLoopStart() above or
  // PostMainMessageLoopStart() below.

  TRACE_EVENT0("startup", "BrowserMainLoop::MainMessageLoopStart");

  if (!base::MessageLoopCurrentForUI::IsSet())
    main_message_loop_ = std::make_unique<base::MessageLoopForUI>();

  InitializeMainThread();
}

void BrowserMainLoop::PostMainMessageLoopStart() {
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:SystemMonitor");
    system_monitor_.reset(new base::SystemMonitor);
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:PowerMonitor");
    std::unique_ptr<base::PowerMonitorSource> power_monitor_source(
        new base::PowerMonitorDeviceSource());
    power_monitor_.reset(
        new base::PowerMonitor(std::move(power_monitor_source)));
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:HighResTimerManager");
    hi_res_timer_manager_.reset(new base::HighResolutionTimerManager);
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:NetworkChangeNotifier");
    network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:ScreenlockMonitor");
    std::unique_ptr<ScreenlockMonitorSource> screenlock_monitor_source =
        std::make_unique<ScreenlockMonitorDeviceSource>();
    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::move(screenlock_monitor_source));
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:MediaFeatures");
    media::InitializeMediaLibrary();
  }
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ContentWebUIController");
    WebUIControllerFactory::RegisterFactory(
        ContentWebUIControllerFactory::GetInstance());
  }

  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:OnlineStateObserver");
    online_state_observer_.reset(new BrowserOnlineStateObserver);
  }

  {
    system_stats_monitor_.reset(
        new base::trace_event::TraceEventSystemStatsMonitor(
            base::ThreadTaskRunnerHandle::Get()));
  }

  {
    base::SetRecordActionTaskRunner(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}));
  }

  if (!features::IsMultiProcessMash()) {
    discardable_shared_memory_manager_ =
        std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();
    // TODO(boliu): kSingleProcess check is a temporary workaround for
    // in-process Android WebView. crbug.com/503724 tracks proper fix.
    if (!parsed_command_line_.HasSwitch(switches::kSingleProcess)) {
      base::DiscardableMemoryAllocator::SetInstance(
          discardable_shared_memory_manager_.get());
    }
  }

  if (parts_)
    parts_->PostMainMessageLoopStart();

#if defined(OS_ANDROID)
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:BrowserMediaPlayerManager");
    if (UsingInProcessGpu()) {
      gpu::ScopedSurfaceRequestConduit::SetInstance(
          ScopedSurfaceRequestManager::GetInstance());
    }
  }

  if (!parsed_command_line_.HasSwitch(
      switches::kDisableScreenOrientationLock)) {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ScreenOrientationProvider");
    screen_orientation_delegate_.reset(
        new ScreenOrientationDelegateAndroid());
  }

  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(
      base::trace_event::CPUFreqMonitor::GetInstance());
#endif

  if (parsed_command_line_.HasSwitch(
          switches::kEnableAggressiveDOMStorageFlushing)) {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:EnableAggressiveCommitDelay");
    DOMStorageArea::EnableAggressiveCommitDelay();
    StorageAreaImpl::EnableAggressiveCommitDelay();
  }

  // Enable memory-infra dump providers.
  InitSkiaEventTracer();
#if !defined(OS_ANDROID)
  if (server_shared_bitmap_manager_) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        server_shared_bitmap_manager_.get(), "viz::ServerSharedBitmapManager",
        base::ThreadTaskRunnerHandle::Get());
  }
#endif
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      sql::SqlMemoryDumpProvider::GetInstance(), "Sql", nullptr);
  network::TransitionalURLLoaderFactoryOwner::DisallowUsageInProcess();
}

int BrowserMainLoop::PreCreateThreads() {
  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::CreateThreads:PreCreateThreads");

    result_code_ = parts_->PreCreateThreads();
  }

  InitializeMemoryManagementComponent();

#if BUILDFLAG(ENABLE_PLUGINS)
  // Prior to any processing happening on the IO thread, we create the
  // plugin service as it is predominantly used from the IO thread,
  // but must be created on the main thread. The service ctor is
  // inexpensive and does not invoke the io_thread() accessor.
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::CreateThreads:PluginService");
    PluginService::GetInstance()->Init();
  }
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Prior to any processing happening on the IO thread, we create the
  // CDM service as it is predominantly used from the IO thread. This must
  // be called on the main thread since it involves file path checks.
  CdmRegistry::GetInstance()->Init();
#endif

#if defined(OS_MACOSX)
  // The WindowResizeHelper allows the UI thread to wait on specific renderer
  // and GPU messages from the IO thread. Initializing it before the IO thread
  // starts ensures the affected IO thread messages always have somewhere to go.
  ui::WindowResizeHelperMac::Get()->Init(base::ThreadTaskRunnerHandle::Get());
#endif

  // 1) Need to initialize in-process GpuDataManager before creating threads.
  // It's unsafe to append the gpu command line switches to the global
  // CommandLine::ForCurrentProcess object after threads are created.
  // 2) Must be after parts_->PreCreateThreads to pick up chrome://flags.
  GpuDataManagerImpl::GetInstance();

#if defined(USE_X11)
  gpu_data_manager_visual_proxy_.reset(new internal::GpuDataManagerVisualProxy(
      GpuDataManagerImpl::GetInstance()));
#endif

#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_ANDROID)
  // Single-process is an unsupported and not fully tested mode, so
  // don't enable it for official Chrome builds (except on Android).
  if (parsed_command_line_.HasSwitch(switches::kSingleProcess))
    RenderProcessHost::SetRunRendererInProcess(true);
#endif

  // Initialize origins that are whitelisted for process isolation.  Must be
  // done after base::FeatureList is initialized, but before any navigations
  // can happen.
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->AddIsolatedOrigins(SiteIsolationPolicy::GetIsolatedOrigins());

  // Record metrics about which site isolation flags have been turned on.
  SiteIsolationPolicy::StartRecordingSiteIsolationFlagUsage();

  return result_code_;
}

void BrowserMainLoop::PreShutdown() {
  parts_->PreShutdown();

  ui::Clipboard::OnPreShutdownForCurrentThread();
}

void BrowserMainLoop::CreateStartupTasks() {
  TRACE_EVENT0("startup", "BrowserMainLoop::CreateStartupTasks");

  DCHECK(!startup_task_runner_);
#if defined(OS_ANDROID)
  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::BindOnce(&BrowserStartupComplete),
      base::ThreadTaskRunnerHandle::Get());
#else
  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::OnceCallback<void(int)>(), base::ThreadTaskRunnerHandle::Get());
#endif
  StartupTask pre_create_threads =
      base::Bind(&BrowserMainLoop::PreCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(pre_create_threads));

  StartupTask create_threads =
      base::Bind(&BrowserMainLoop::CreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(create_threads));

  StartupTask post_create_threads =
      base::Bind(&BrowserMainLoop::PostCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(post_create_threads));

  StartupTask browser_thread_started = base::Bind(
      &BrowserMainLoop::BrowserThreadsStarted, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(browser_thread_started));

  StartupTask pre_main_message_loop_run = base::Bind(
      &BrowserMainLoop::PreMainMessageLoopRun, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(pre_main_message_loop_run));

#if defined(OS_ANDROID)
  if (parameters_.ui_task) {
    // Running inside browser tests, which relies on synchronous start.
    startup_task_runner_->RunAllTasksNow();
  } else {
    startup_task_runner_->StartRunningTasksAsync();
  }
#else
  startup_task_runner_->RunAllTasksNow();
#endif
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserMainLoop::GetResizeTaskRunner() {
#if defined(OS_MACOSX)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  // In tests, WindowResizeHelperMac task runner might not be initialized.
  return task_runner ? task_runner : base::ThreadTaskRunnerHandle::Get();
#else
  return base::ThreadTaskRunnerHandle::Get();
#endif
}

gpu::GpuChannelEstablishFactory*
BrowserMainLoop::gpu_channel_establish_factory() const {
  return BrowserGpuChannelHostFactory::instance();
}

#if defined(OS_ANDROID)
void BrowserMainLoop::SynchronouslyFlushStartupTasks() {
  startup_task_runner_->RunAllTasksNow();
}
#endif  // OS_ANDROID

int BrowserMainLoop::CreateThreads() {
  TRACE_EVENT0("startup,rail", "BrowserMainLoop::CreateThreads");

  // Release the TaskScheduler's threads.
  scoped_execution_fence_.reset();

  // The |io_thread| can have optionally been injected into Init(), but if not,
  // create it here. Thre thread is only tagged as BrowserThread::IO here in
  // order to prevent any code from statically posting to it before
  // CreateThreads() (as such maintaining the invariant that PreCreateThreads()
  // et al. "happen-before" BrowserThread::IO is "brought up").
  if (!io_thread_) {
    io_thread_ = BrowserProcessSubThread::CreateIOThread();
  }
  io_thread_->RegisterAsBrowserThread();

  created_threads_ = true;
  return result_code_;
}

int BrowserMainLoop::PostCreateThreads() {
  if (parts_) {
    TRACE_EVENT0("startup", "BrowserMainLoop::PostCreateThreads");
    parts_->PostCreateThreads();
  }

  return result_code_;
}

int BrowserMainLoop::PreMainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Let screen instance be overridable by parts.
  ui::SetScreenAndroid();
#endif

  if (parts_) {
    TRACE_EVENT0("startup",
        "BrowserMainLoop::CreateThreads:PreMainMessageLoopRun");

    parts_->PreMainMessageLoopRun();
  }

  // If the UI thread blocks, the whole UI is unresponsive.
  // Do not allow unresponsive tasks from the UI thread.
  base::DisallowUnresponsiveTasks();
  return result_code_;
}

void BrowserMainLoop::RunMainMessageLoopParts() {
  // Don't use the TRACE_EVENT0 macro because the tracing infrastructure doesn't
  // expect synchronous events around the main loop of a thread.
  TRACE_EVENT_ASYNC_BEGIN0("toplevel", "BrowserMain:MESSAGE_LOOP", this);

  bool ran_main_loop = false;
  if (parts_)
    ran_main_loop = parts_->MainMessageLoopRun(&result_code_);

  if (!ran_main_loop)
    MainMessageLoopRun();

  TRACE_EVENT_ASYNC_END0("toplevel", "BrowserMain:MESSAGE_LOOP", this);
}

void BrowserMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }
  TRACE_EVENT0("shutdown", "BrowserMainLoop::ShutdownThreadsAndCleanUp");

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::ThreadRestrictions::SetIOAllowed(true);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          base::IgnoreResult(&base::ThreadRestrictions::SetIOAllowed), true));

#if defined(OS_ANDROID)
  g_browser_main_loop_shutting_down = true;
#endif

  if (RenderProcessHost::run_renderer_in_process())
    RenderProcessHostImpl::ShutDownInProcessRenderer();

  if (parts_) {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:PostMainMessageLoopRun");
    parts_->PostMainMessageLoopRun();
  }

  system_stats_monitor_.reset();

  // Cancel pending requests and prevent new requests.
  if (resource_dispatcher_host_) {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:ResourceDispatcherHost");
    resource_dispatcher_host_->Shutdown();
  }
  // Request shutdown to clean up allocated resources on the IO thread.
  if (midi_service_) {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:MidiService");
    midi_service_->Shutdown();
  }

  TRACE_EVENT0("shutdown",
               "BrowserMainLoop::Subsystem:SpeechRecognitionManager");
  io_thread_->task_runner()->DeleteSoon(FROM_HERE,
                                        speech_recognition_manager_.release());

  memory_pressure_monitor_.reset();

#if defined(OS_MACOSX)
  BrowserCompositorMac::DisableRecyclingForShutdown();
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
  {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:ImageTransportFactory");
    ImageTransportFactory::Terminate();
  }
#endif

#if !defined(OS_ANDROID)
  host_frame_sink_manager_.reset();
  frame_sink_manager_impl_.reset();
  compositing_mode_reporter_impl_.reset();
  server_shared_bitmap_manager_.reset();
#endif

// The device monitors are using |system_monitor_| as dependency, so delete
// them before |system_monitor_| goes away.
// On Mac and windows, the monitor needs to be destroyed on the same thread
// as they were created. On Linux, the monitor will be deleted when IO thread
// goes away.
#if defined(OS_WIN)
  system_message_window_.reset();
#elif defined(OS_MACOSX)
  device_monitor_mac_.reset();
#endif

  if (BrowserGpuChannelHostFactory::instance())
    BrowserGpuChannelHostFactory::instance()->CloseChannel();

  // Shutdown the Service Manager and IPC.
  service_manager_context_.reset();
  mojo_ipc_support_.reset();

  if (save_file_manager_)
    save_file_manager_->Shutdown();

  {
    base::ThreadRestrictions::ScopedAllowWait allow_wait_for_join;
    {
      TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:IOThread");
      ResetThread_IO(std::move(io_thread_));
    }

    {
      TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:TaskScheduler");
      base::TaskScheduler::GetInstance()->Shutdown();
    }
  }

  // Must happen after the IO thread is shutdown since this may be accessed from
  // it.
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:GPUChannelFactory");
    if (BrowserGpuChannelHostFactory::instance()) {
      BrowserGpuChannelHostFactory::Terminate();
    }
  }

  // Must happen after the I/O thread is shutdown since this class lives on the
  // I/O thread and isn't threadsafe.
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:GamepadService");
    device::GamepadService::GetInstance()->Terminate();
  }
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:DeleteDataSources");
    URLDataManager::DeleteDataSources();
  }
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:AudioMan");
    if (audio_manager_ && !audio_manager_->Shutdown()) {
      // Intentionally leak AudioManager if shutdown failed.
      // We might run into various CHECK(s) in AudioManager destructor.
      ignore_result(audio_manager_.release());
      // |user_input_monitor_| may be in use by stray streams in case
      // AudioManager shutdown failed.
      ignore_result(user_input_monitor_.release());
    }

    // Leaking AudioSystem: we cannot correctly destroy it since Audio service
    // connection in there is bound to IO thread.
    ignore_result(audio_system_.release());
  }

  if (parts_) {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:PostDestroyThreads");
    parts_->PostDestroyThreads();
  }
}

media::AudioManager* BrowserMainLoop::audio_manager() const {
  DCHECK(audio_manager_) << "AudioManager is not instantiated - running the "
                            "audio service out of process?";
  return audio_manager_.get();
}

#if !defined(OS_ANDROID)
viz::FrameSinkManagerImpl* BrowserMainLoop::GetFrameSinkManager() const {
  return frame_sink_manager_impl_.get();
}

viz::ServerSharedBitmapManager* BrowserMainLoop::GetServerSharedBitmapManager()
    const {
  return server_shared_bitmap_manager_.get();
}
#endif

void BrowserMainLoop::GetCompositingModeReporter(
    viz::mojom::CompositingModeReporterRequest request) {
#if defined(OS_ANDROID)
  // Android doesn't support non-gpu compositing modes, and doesn't make a
  // CompositingModeReporter.
  return;
#else
  if (features::IsMultiProcessMash()) {
    // Mash == ChromeOS, which doesn't support software compositing, so no need
    // to report compositing mode.
    return;
  }

  compositing_mode_reporter_impl_->BindRequest(std::move(request));
#endif
}

void BrowserMainLoop::StopStartupTracingTimer() {
  startup_trace_timer_.Stop();
}

void BrowserMainLoop::InitializeMainThread() {
  TRACE_EVENT0("startup", "BrowserMainLoop::InitializeMainThread");
  base::PlatformThread::SetName("CrBrowserMain");

  // Register the main thread. The main thread's task runner should already have
  // been initialized in MainMessageLoopStart() (or before if
  // MessageLoopCurrent::Get() was externally provided).
  DCHECK(base::ThreadTaskRunnerHandle::IsSet());
  main_thread_.reset(new BrowserThreadImpl(
      BrowserThread::UI, base::ThreadTaskRunnerHandle::Get()));
}

int BrowserMainLoop::BrowserThreadsStarted() {
  TRACE_EVENT0("startup", "BrowserMainLoop::BrowserThreadsStarted");

  // Bring up Mojo IPC and the embedded Service Manager as early as possible.
  // Initializaing mojo requires the IO thread to have been initialized first,
  // so this cannot happen any earlier than now.
  InitializeMojo();

#if BUILDFLAG(ENABLE_MUS)
  if (features::IsUsingWindowService()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableSurfaceSynchronization);
  }
#endif

  HistogramSynchronizer::GetInstance();

  // cc assumes a single client name for metrics in a process, which is
  // is inconsistent with single process mode where both the renderer and
  // browser compositor run in the same process. In this case, avoid
  // initializing with a browser metric name to ensure we record metrics for the
  // renderer compositor.
  // Note that since single process mode is only used by webview in practice,
  // which doesn't have a browser compositor, this is not required anyway.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    cc::SetClientNameForMetrics("Browser");
  }

  // Initialize the GPU shader cache. This needs to be initialized before
  // BrowserGpuChannelHostFactory below, since that depends on an initialized
  // ShaderCacheFactory.
  InitShaderCacheFactorySingleton(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));

  // Initialize the FontRenderParams on IO thread. This needs to be initialized
  // before gpu process initialization below.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &viz::GpuHostImpl::InitFontRenderParams,
          gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr)));

  // If ash/ws is not hosting viz, then the browser must.
  bool browser_is_viz_host = !features::IsMultiProcessMash();

  bool always_uses_gpu = true;
  bool established_gpu_channel = false;
#if defined(OS_ANDROID)
  // TODO(crbug.com/439322): This should be set to |true|.
  established_gpu_channel = false;
  always_uses_gpu = ShouldStartGpuProcessOnBrowserStartup();
  BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
#else
  established_gpu_channel = true;
  if (parsed_command_line_.HasSwitch(switches::kDisableGpu) ||
      parsed_command_line_.HasSwitch(switches::kDisableGpuCompositing) ||
      parsed_command_line_.HasSwitch(switches::kDisableGpuEarlyInit) ||
      !browser_is_viz_host) {
    established_gpu_channel = always_uses_gpu = false;
  }

  if (browser_is_viz_host) {
    host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();
    BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
    compositing_mode_reporter_impl_ =
        std::make_unique<viz::CompositingModeReporterImpl>();

    if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor)) {
      auto transport_factory = std::make_unique<VizProcessTransportFactory>(
          BrowserGpuChannelHostFactory::instance(), GetResizeTaskRunner(),
          compositing_mode_reporter_impl_.get());
      transport_factory->ConnectHostFrameSinkManager();
      ImageTransportFactory::SetFactory(std::move(transport_factory));
    } else {
      server_shared_bitmap_manager_ =
          std::make_unique<viz::ServerSharedBitmapManager>();
      frame_sink_manager_impl_ = std::make_unique<viz::FrameSinkManagerImpl>(
          server_shared_bitmap_manager_.get(),
          switches::GetDeadlineToSynchronizeSurfaces());

      surface_utils::ConnectWithLocalFrameSinkManager(
          host_frame_sink_manager_.get(), frame_sink_manager_impl_.get(),
          base::ThreadTaskRunnerHandle::Get());

      ImageTransportFactory::SetFactory(
          std::make_unique<GpuProcessTransportFactory>(
              BrowserGpuChannelHostFactory::instance(),
              compositing_mode_reporter_impl_.get(),
              server_shared_bitmap_manager_.get(), GetResizeTaskRunner()));
    }
  }

#if defined(USE_AURA)
  // In single process mash mode the aura::Env created here uses the
  // WindowService, and needs to use the context-factory from aura.
  if (browser_is_viz_host && !features::IsSingleProcessMash()) {
    env_->set_context_factory(GetContextFactory());
    env_->set_context_factory_private(GetContextFactoryPrivate());
  }
#endif  // defined(USE_AURA)
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);
#endif

  {
    TRACE_EVENT0("startup", "BrowserThreadsStarted::Subsystem:AudioMan");
    InitializeAudio();
  }

  {
    TRACE_EVENT0("startup", "BrowserThreadsStarted::Subsystem:MidiService");
    midi_service_.reset(new midi::MidiService);
  }

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kHighDynamicRange))
    HDRProxy::Initialize();
  system_message_window_.reset(new media::SystemMessageWindowWin);
#elif defined(OS_LINUX) && defined(USE_UDEV)
  device_monitor_linux_.reset(
      new media::DeviceMonitorLinux(io_thread_->task_runner()));
#elif defined(OS_MACOSX)
  // On Mac, the audio task runner must belong to the main thread.
  // See audio_thread_impl.cc and https://crbug.com/158170.
  DCHECK(!audio_manager_ ||
         audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  device_monitor_mac_.reset(
      new media::DeviceMonitorMac(base::ThreadTaskRunnerHandle::Get()));
#endif

  // Instantiated once using CreateSingletonInstance(), and accessed only using
  // GetInstance(), which is not allowed to create the object. This allows us
  // to ensure that it cannot be used before objects it relies on have been
  // created; namely, WebRtcEventLogManager.
  // Allowed to leak when the browser exits.
  WebRTCInternals::CreateSingletonInstance();

  // RDH needs the IO thread to be created
  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitResourceDispatcherHost");
    // TODO(ananta)
    // We register an interceptor on the ResourceDispatcherHostImpl instance to
    // intercept requests to create handlers for download requests. We need to
    // find a better way to achieve this. Ideally we don't want knowledge of
    // downloads in ResourceDispatcherHostImpl.
    // We pass the task runners for the UI and IO threads as a stopgap approach
    // for now. Eventually variants of these runners would be available in the
    // network service.
    resource_dispatcher_host_.reset(new ResourceDispatcherHostImpl(
        base::Bind(&DownloadResourceHandler::Create),
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
        !parsed_command_line_.HasSwitch(switches::kDisableResourceScheduler)));
    GetContentClient()->browser()->ResourceDispatcherHostCreated();

    loader_delegate_.reset(new LoaderDelegateImpl());
    resource_dispatcher_host_->SetLoaderDelegate(loader_delegate_.get());
  }

  // MediaStreamManager needs the IO thread to be created.
  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitMediaStreamManager");

    scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner =
        audio_manager_ ? audio_manager_->GetTaskRunner() : nullptr;

#if defined(OS_MACOSX)
    // On Mac, the audio task runner must belong to the main thread.
    // See audio_thread_impl.cc and https://crbug.com/158170.
    if (audio_task_runner) {
      DCHECK(audio_task_runner->BelongsToCurrentThread());
    } else {
      audio_task_runner = base::ThreadTaskRunnerHandle::Get();
    }
#endif

    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::move(audio_task_runner));
  }

  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted:InitSpeechRecognition");
    speech_recognition_manager_.reset(new SpeechRecognitionManagerImpl(
        audio_system_.get(), media_stream_manager_.get()));
  }

  {
    TRACE_EVENT0(
        "startup",
        "BrowserMainLoop::BrowserThreadsStarted::InitUserInputMonitor");
    user_input_monitor_ = media::UserInputMonitor::Create(
        io_thread_->task_runner(), base::ThreadTaskRunnerHandle::Get());
  }

  {
    TRACE_EVENT0("startup",
      "BrowserMainLoop::BrowserThreadsStarted::SaveFileManager");
    save_file_manager_ = new SaveFileManager();
  }

  // Alert the clipboard class to which threads are allowed to access the
  // clipboard:
  std::vector<base::PlatformThreadId> allowed_clipboard_threads;
  // The current thread is the UI thread.
  allowed_clipboard_threads.push_back(base::PlatformThread::CurrentId());
#if defined(OS_WIN)
  // On Windows, clipboard is also used on the IO thread.
  allowed_clipboard_threads.push_back(io_thread_->GetThreadId());
#endif
  ui::Clipboard::SetAllowedThreads(allowed_clipboard_threads);

  if (GpuDataManagerImpl::GetInstance()->GpuProcessStartAllowed() &&
      !established_gpu_channel && always_uses_gpu && browser_is_viz_host) {
    TRACE_EVENT_INSTANT0("gpu", "Post task to launch GPU process",
                         TRACE_EVENT_SCOPE_THREAD);
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(base::IgnoreResult(&GpuProcessHost::Get),
                       GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                       true /* force_create */));
  }

#if defined(OS_WIN)
  if (!parsed_command_line_.HasSwitch(
          switches::kDisableGpuProcessForDX12VulkanInfoCollection)) {
    GpuDataManagerImpl::GetInstance()->RequestGpuSupportedRuntimeVersion();
  }
#endif

#if defined(OS_MACOSX)
  ThemeHelperMac::GetInstance();
  SystemHotkeyHelperMac::GetInstance()->DeferredLoadSystemHotkeys();
#endif  // defined(OS_MACOSX)

#if !defined(OS_ANDROID)
  responsiveness_watcher_ = new responsiveness::Watcher;
  responsiveness_watcher_->SetUp();
#endif

#if defined(OS_ANDROID)
  media::SetMediaDrmBridgeClient(GetContentClient()->GetMediaDrmBridgeClient());
#endif

#if defined(ENABLE_IPC_FUZZER)
  SetFileUrlPathAliasForIpcFuzzer();
#endif
  return result_code_;
}

bool BrowserMainLoop::UsingInProcessGpu() const {
  return parsed_command_line_.HasSwitch(switches::kSingleProcess) ||
         parsed_command_line_.HasSwitch(switches::kInProcessGPU);
}

void BrowserMainLoop::InitializeMemoryManagementComponent() {
  memory_pressure_monitor_ = CreateMemoryPressureMonitor(parsed_command_line_);

  std::unique_ptr<SwapMetricsDriver::Delegate> delegate(
      base::WrapUnique<SwapMetricsDriver::Delegate>(
          new SwapMetricsDelegateUma()));

#if !defined(OS_FUCHSIA)
  swap_metrics_driver_ =
      SwapMetricsDriver::Create(std::move(delegate), kSwapMetricsInterval);
  if (swap_metrics_driver_)
    swap_metrics_driver_->Start();
#endif  // !defined(OS_FUCHSIA)
}

bool BrowserMainLoop::InitializeToolkit() {
  TRACE_EVENT0("startup", "BrowserMainLoop::InitializeToolkit");

  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.
  // TODO(stevenjb): Move platform specific code into platform specific Parts
  // (Need to add InitializeToolkit stage to BrowserParts).
  // See also GTK setup in EarlyInitialization, above, and associated comments.

#if defined(OS_WIN)
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  if (!InitCommonControlsEx(&config))
    PLOG(FATAL);
#endif

#if defined(USE_AURA)

#if defined(USE_X11)
  if (!parsed_command_line_.HasSwitch(switches::kHeadless) &&
      !gfx::GetXDisplay()) {
    LOG(ERROR) << "Unable to open X display.";
    return false;
  }
#endif

  // Env creates the compositor. Aura widgets need the compositor to be created
  // before they can be initialized by the browser.
  env_ = aura::Env::CreateInstance(features::IsUsingWindowService()
                                       ? aura::Env::Mode::MUS
                                       : aura::Env::Mode::LOCAL);
#endif  // defined(USE_AURA)

  if (parts_)
    parts_->ToolkitInitialized();

  return true;
}

void BrowserMainLoop::MainMessageLoopRun() {
#if defined(OS_ANDROID)
  // Android's main message loop is the Java message loop.
  NOTREACHED();
#else
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  if (parameters_.ui_task) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  *parameters_.ui_task);
  }

  base::RunLoop run_loop;
  parts_->PreDefaultMainMessageLoopRun(run_loop.QuitClosure());
  run_loop.Run();
#endif
}

void BrowserMainLoop::InitializeMojo() {
  if (!parsed_command_line_.HasSwitch(switches::kSingleProcess)) {
    // Disallow mojo sync calls in the browser process. Note that we allow sync
    // calls in single-process mode since renderer IPCs are made from a browser
    // thread.
    mojo::SyncCallRestrictions::DisallowSyncCall();
  }

  mojo_ipc_support_.reset(new mojo::core::ScopedIPCSupport(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST));

  service_manager_context_.reset(
      new ServiceManagerContext(io_thread_->task_runner()));
  ServiceManagerContext::StartBrowserConnection();
#if defined(OS_MACOSX)
  mojo::core::SetMachPortProvider(MachBroker::GetInstance());
#endif  // defined(OS_MACOSX)
  GetContentClient()->OnServiceManagerConnected(
      ServiceManagerConnection::GetForProcess());

  // Ensure that any NavigableContentsViews constructed in the browser process
  // know they're running in the same process as the service.
  content::NavigableContentsView::SetClientRunningInServiceProcess();

  tracing_controller_ = std::make_unique<content::TracingControllerImpl>();
  content::BackgroundTracingManagerImpl::GetInstance()
      ->AddMetadataGeneratorFunction();

  // Registers the browser process as a memory-instrumentation client, so
  // that data for the browser process will be available in memory dumps.
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  memory_instrumentation::ClientProcessImpl::Config config(
      connector, resource_coordinator::mojom::kServiceName,
      memory_instrumentation::mojom::ProcessType::BROWSER);
  memory_instrumentation::ClientProcessImpl::CreateInstance(config);

  // Start startup tracing through TracingController's interface. TraceLog has
  // been enabled in content_main_runner where threads are not available. Now We
  // need to start tracing for all other tracing agents, which require threads.
  auto* trace_startup_config = tracing::TraceStartupConfig::GetInstance();
  if (trace_startup_config->IsEnabled()) {
    // This checks kTraceConfigFile switch.
    TracingController::GetInstance()->StartTracing(
        trace_startup_config->GetTraceConfig(),
        TracingController::StartTracingDoneCallback());
  } else if (parsed_command_line_.HasSwitch(switches::kTraceToConsole)) {
    TracingController::GetInstance()->StartTracing(
        tracing::GetConfigForTraceToConsole(),
        TracingController::StartTracingDoneCallback());
  }
  // Start tracing to a file for certain duration if needed. Only do this after
  // starting the main message loop to avoid calling
  // MessagePumpForUI::ScheduleWork() before MessagePumpForUI::Start() as it
  // will crash the browser.
  if (trace_startup_config->IsTracingStartupForDuration()) {
    TRACE_EVENT0("startup", "BrowserMainLoop::InitStartupTracingForDuration");
    InitStartupTracingForDuration();
  }

  if (parts_) {
    parts_->ServiceManagerConnectionStarted(
        ServiceManagerConnection::GetForProcess());
  }
}

base::FilePath BrowserMainLoop::GetStartupTraceFileName() const {
  base::FilePath trace_file;

  trace_file = tracing::TraceStartupConfig::GetInstance()->GetResultFile();
  if (trace_file.empty()) {
#if defined(OS_ANDROID)
    TracingControllerAndroid::GenerateTracingFilePath(&trace_file);
#else
    // Default to saving the startup trace into the current dir.
    trace_file = base::FilePath().AppendASCII("chrometrace.log");
#endif
  }

  return trace_file;
}

void BrowserMainLoop::InitStartupTracingForDuration() {
  DCHECK(tracing::TraceStartupConfig::GetInstance()
             ->IsTracingStartupForDuration());

  startup_trace_file_ = GetStartupTraceFileName();

  startup_trace_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          tracing::TraceStartupConfig::GetInstance()->GetStartupDuration()),
      this, &BrowserMainLoop::EndStartupTracing);
}

void BrowserMainLoop::EndStartupTracing() {
  // Do nothing if startup tracing is already stopped.
  if (!tracing::TraceStartupConfig::GetInstance()->IsEnabled())
    return;

  TracingController::GetInstance()->StopTracing(
      TracingController::CreateFileEndpoint(
          startup_trace_file_,
          base::Bind(OnStoppedStartupTracing, startup_trace_file_)));
}

void BrowserMainLoop::InitializeAudio() {
  DCHECK(!audio_manager_);

  audio_manager_ = GetContentClient()->browser()->CreateAudioManager(
      MediaInternals::GetInstance());
  DCHECK_EQ(!!audio_manager_,
            GetContentClient()->browser()->OverridesAudioManager());

  // Do not initialize |audio_manager_| if running out of process.
  if (!audio_manager_ &&
      !base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess)) {
    audio_manager_ =
        media::AudioManager::Create(std::make_unique<media::AudioThreadImpl>(),
                                    MediaInternals::GetInstance());
    CHECK(audio_manager_);
  }

  // Iff |audio_manager_| is instantiated, the audio service will run
  // in-process. Complete the setup for that:
  if (audio_manager_) {
    AudioMirroringManager* const mirroring_manager =
        AudioMirroringManager::GetInstance();
    audio_manager_->SetDiverterCallbacks(
        mirroring_manager->GetAddDiverterCallback(),
        mirroring_manager->GetRemoveDiverterCallback());

    TRACE_EVENT_INSTANT0("startup", "Starting Audio service task runner",
                         TRACE_EVENT_SCOPE_THREAD);
    ServiceManagerContext::GetAudioServiceRunner()->StartWithTaskRunner(
        audio_manager_->GetTaskRunner());
  }

  if (base::FeatureList::IsEnabled(features::kAudioServiceLaunchOnStartup)) {
    // Schedule the audio service startup on the main thread.
    BrowserThread::PostAfterStartupTask(
        FROM_HERE,
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
        base::BindOnce([]() {
          TRACE_EVENT0("audio", "Starting audio service");
          ServiceManagerConnection* connection =
              content::ServiceManagerConnection::GetForProcess();
          if (connection) {
            // The browser is not shutting down: |connection| would be null
            // otherwise.
            connection->GetConnector()->StartService(
                audio::mojom::kServiceName);
          }
        }));
  }

  audio_system_ = audio::CreateAudioSystem(
      content::ServiceManagerConnection::GetForProcess()
          ->GetConnector()
          ->Clone());
  CHECK(audio_system_);
}

bool BrowserMainLoop::AudioServiceOutOfProcess() const {
  // Returns true iff kAudioServiceOutOfProcess feature is enabled and if the
  // embedder does not provide its own in-process AudioManager.
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

}  // namespace content
