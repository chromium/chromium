// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main_loop.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/base_switches.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/pending_task.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/system_monitor.h"
#include "base/task/current_thread.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/initialization_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/histograms.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "components/power_monitor/make_power_monitor_device_source.h"
#include "components/services/storage/dom_storage/storage_area_impl.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/fake_crash.h"
#include "components/viz/host/compositing_mode_reporter_impl.h"
#include "components/viz/host/gpu_host_impl.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/compositor/viz_process_transport_factory.h"
#include "content/browser/download/save_file_manager.h"
#include "content/browser/field_trial_synchronizer.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/browser_gpu_client_delegate.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_disk_cache_factory.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/browser/memory_pressure/user_level_memory_pressure_signal_generator.h"
#include "content/browser/metrics/histogram_synchronizer.h"
#include "content/browser/network/browser_online_state_observer.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/scheduler/responsiveness/watcher.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"
#include "content/browser/sms/sms_provider.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/tts_controller_impl.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_task_runner.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/startup_tracing_controller.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/browser/utility_process_host.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/browser/webui/content_web_ui_configs.h"
#include "content/browser/webui/url_data_manager.h"
#include "content/common/content_switches_internal.h"
#include "content/common/features.h"
#include "content/common/pseudonymization_salt.h"
#include "content/common/skia_utils.h"
#include "content/common/thread_pool_util.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "crypto/crypto_buildflags.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "device/gamepad/gamepad_service.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system.h"
#include "media/audio/audio_thread_impl.h"
#include "media/base/user_input_monitor.h"
#include "media/media_buildflags.h"
#include "media/midi/midi_service.h"
#include "media/mojo/buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/network_change_notifier.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/ssl/ssl_config_service.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/audio/service.h"
#include "services/data_decoder/public/cpp/service_provider.h"
#include "services/data_decoder/public/mojom/data_decoder_service.mojom.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/transitional_url_loader_factory_owner.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "services/video_capture/public/cpp/features.h"
#include "skia/ext/event_tracer_impl.h"
#include "skia/ext/legacy_display_globals.h"
#include "skia/ext/skia_memory_dump_provider.h"
#include "sql/sql_memory_dump_provider.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/display/display_features.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/switches.h"

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
#include "content/browser/compositor/image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/public/browser/context_factory.h"
#include "ui/aura/env.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/trace_event/cpufreq_monitor_android.h"
#include "components/tracing/common/graphics_memory_dump_provider_android.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/browser/android/launcher_thread.h"
#include "content/browser/android/scoped_surface_request_manager.h"
#include "content/browser/android/tracing_controller_android.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_android.h"
#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"
#include "media/base/android/media_drm_bridge_client.h"
#include "ui/android/screen_android.h"
#include "ui/display/screen.h"
#include "ui/gl/gl_surface.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "content/browser/renderer_host/browser_compositor_view_mac.h"
#include "content/browser/theme_helper_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include "base/threading/platform_thread_win.h"
#include "net/base/winsock_init.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#endif

#if defined(USE_GLIB)
#include <glib-object.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "media/device_monitors/system_message_window_win.h"
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
#include "media/device_monitors/device_monitor_udev.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/job.h>

#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_host_linux.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) || BUILDFLAG(IS_ANDROID)
#include "content/browser/media/cdm_registry_impl.h"
#endif

#if BUILDFLAG(USE_NSS_CERTS)
#include "crypto/nss_util.h"
#endif

#if defined(ENABLE_IPC_FUZZER) && BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
#include "mojo/public/cpp/bindings/lib/test_random_mojo_delays.h"
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
    NOTREACHED_IN_MIGRATION();
    LOG(DFATAL) << log_domain << ": " << message;
  }
}

static void SetUpGLibLogHandler() {
  // Register GLib-handled assertions to go through our logging system.
  const char* const kLogDomains[] = {nullptr, "Gtk", "Gdk", "GLib",
                                     "GLib-GObject"};
  for (const auto* domain : kLogDomains) {
    g_log_set_handler(
        domain,
        static_cast<GLogLevelFlags>(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL |
                                    G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
                                    G_LOG_LEVEL_WARNING),
        GLibLogHandler, nullptr);
  }
}
#endif  // defined(USE_GLIB)

// NOINLINE so it's possible to tell what thread was unresponsive by inspecting
// the callstack.
NOINLINE void ResetThread_IO(
    std::unique_ptr<BrowserProcessIOThread> io_thread) {
  io_thread.reset();
}

enum WorkerPoolType : size_t {
  BACKGROUND = 0,
  BACKGROUND_BLOCKING,
  FOREGROUND,
  FOREGROUND_BLOCKING,
  WORKER_POOL_COUNT  // Always last.
};

#if BUILDFLAG(IS_FUCHSIA)
// Create and register the job which will contain all child processes
// of the browser process as well as their descendents.
void InitDefaultJob() {
  zx::job job;
  zx_status_t result = zx::job::create(*zx::job::default_job(), 0, &job);
  ZX_CHECK(ZX_OK == result, result) << "zx_job_create";
  base::SetDefaultJob(std::move(job));
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if defined(ENABLE_IPC_FUZZER)
bool GetBuildDirectory(base::FilePath* result) {
  if (!base::PathService::Get(base::DIR_EXE, result))
    return false;

#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
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

  std::unique_ptr<memory_pressure::MultiSourceMemoryPressureMonitor> monitor;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  monitor =
      std::make_unique<memory_pressure::MultiSourceMemoryPressureMonitor>();
#endif
  // No memory monitor on other platforms...

  if (monitor)
    monitor->MaybeStartPlatformVoter();

  return monitor;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
mojo::PendingRemote<data_decoder::mojom::BleScanParser> GetBleScanParser() {
  static base::NoDestructor<data_decoder::DataDecoder> decoder;
  mojo::PendingRemote<data_decoder::mojom::BleScanParser> ble_scan_parser;
  decoder->GetService()->BindBleScanParser(
      ble_scan_parser.InitWithNewPipeAndPassReceiver());
  return ble_scan_parser;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class OopDataDecoder : public data_decoder::ServiceProvider {
 public:
  OopDataDecoder() { data_decoder::ServiceProvider::Set(this); }

  OopDataDecoder(const OopDataDecoder&) = delete;
  OopDataDecoder& operator=(const OopDataDecoder&) = delete;

  ~OopDataDecoder() override { data_decoder::ServiceProvider::Set(nullptr); }

  // data_decoder::ServiceProvider implementation:
  void BindDataDecoderService(
      mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver)
      override {
    ServiceProcessHost::Launch(
        std::move(receiver),
        ServiceProcessHost::Options()
            .WithDisplayName("Data Decoder Service")
            .Pass());
  }
};

void BindHidManager(mojo::PendingReceiver<device::mojom::HidManager> receiver) {
#if !BUILDFLAG(IS_ANDROID)
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BindHidManager, std::move(receiver)));
    return;
  }

  GetDeviceService().BindHidManager(std::move(receiver));
#endif
}

uint32_t GenerateBrowserSalt() {
  uint32_t salt;
  do {
    salt = base::RandUint64();
  } while (salt == 0);

  return salt;
}

std::string GetRelatedWebsiteSetSwitch() {
  // `kUseFirstPartySet` switch is being deprecated in favor of
  // `kUseRelatedWebsiteSet` switch. Both switches are supported during the
  // transition period with `kUseRelatedWebsiteSet` taking precedence.
  base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
  if (commandLine->HasSwitch(network::switches::kUseRelatedWebsiteSet)) {
    return commandLine->GetSwitchValueASCII(
        network::switches::kUseRelatedWebsiteSet);
  }
  return commandLine->GetSwitchValueASCII(network::switches::kUseFirstPartySet);
}

}  // namespace

// The currently-running BrowserMainLoop.  There can be one or zero.
BrowserMainLoop* g_current_browser_main_loop = nullptr;

#if BUILDFLAG(IS_ANDROID)

namespace {
// Whether or not BrowserMainLoop::CreateStartupTasks() posts any tasks.
bool g_post_startup_tasks = true;
}  // namespace

// static
void BrowserMainLoop::EnableStartupTasks(bool enabled) {
  g_post_startup_tasks = enabled;
}

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
    MainFunctionParams parameters,
    std::unique_ptr<base::ThreadPoolInstance::ScopedExecutionFence>
        scoped_execution_fence)
    : parameters_(std::move(parameters)),
      parsed_command_line_(*parameters_.command_line),
      result_code_(RESULT_CODE_NORMAL_EXIT),
      created_threads_(false),
      scoped_execution_fence_(std::move(scoped_execution_fence))
#if !BUILDFLAG(IS_ANDROID)
      ,
      // TODO(fdoray): Create the fence on Android too. Not enabled yet because
      // tests timeout. https://crbug.com/887407
      scoped_best_effort_execution_fence_(std::in_place)
#endif
{
  DCHECK(!g_current_browser_main_loop);
  DCHECK(scoped_execution_fence_)
      << "ThreadPool must be halted before kicking off content.";
  g_current_browser_main_loop = this;
}

BrowserMainLoop::~BrowserMainLoop() {
  DCHECK_EQ(this, g_current_browser_main_loop);
  ui::Clipboard::DestroyClipboardForCurrentThread();
  g_current_browser_main_loop = nullptr;
}

void BrowserMainLoop::Init() {
  TRACE_EVENT0("startup", "BrowserMainLoop::Init");

  if (parameters_.startup_data) {
    StartupDataImpl* startup_data =
        static_cast<StartupDataImpl*>(parameters_.startup_data.get());

    // This is always invoked before |io_thread_| is initialized (i.e. never
    // resets it). The thread owned by the data will be registered as
    // BrowserThread::IO in CreateThreads() instead of creating a brand new
    // thread.
    DCHECK(!io_thread_);
    io_thread_ = std::move(startup_data->io_thread);

    DCHECK(!mojo_ipc_support_);
    mojo_ipc_support_ = std::move(startup_data->mojo_ipc_support);

    // The StartupDataImpl was destined to BrowserMainLoop, do not pass it
    // forward.
    parameters_.startup_data.reset();
  }

  parts_ = GetContentClient()->browser()->CreateBrowserMainParts(
      !!parameters_.ui_task);
}

// BrowserMainLoop stages ==================================================

int BrowserMainLoop::EarlyInitialization() {
  TRACE_EVENT0("startup", "BrowserMainLoop::EarlyInitialization");

#if BUILDFLAG(USE_ZYGOTE)
  // The initialization of the sandbox host ends up with forking the Zygote
  // process and requires no thread been forked. The initialization has happened
  // by now since a thread to start the ServiceManager has been created
  // before the browser main loop starts.
  DCHECK(SandboxHostLinux::GetInstance()->IsInitialized());
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
    if (pre_early_init_error_code != RESULT_CODE_NORMAL_EXIT)
      return pre_early_init_error_code;
  }

#if BUILDFLAG(IS_WIN)
  // This assumes FeatureList is initialized, and must happen before
  // SetCurrentThreadType() below.
  base::InitializePlatformThreadFeatures();
#endif

  // SetCurrentThreadType relies on CurrentUIThread on some platforms. The
  // MessagePumpForUI needs to be bound to the main thread by this point.
  DCHECK(base::CurrentUIThread::IsSet());
  base::PlatformThread::SetCurrentThreadType(
      base::ThreadType::kDisplayCritical);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
  // We use quite a few file descriptors for our IPC as well as disk the disk
  // cache, and the default limit on Apple is low (256), so bump it up.

  // Same for Linux. The default various per distro, but it is 1024 on Fedora.
  // Low soft limits combined with liberal use of file descriptors means power
  // users can easily hit this limit with many open tabs. Bump up the limit to
  // an arbitrarily high number. See https://crbug.com/539567
  base::IncreaseFdLimitTo(8192);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  net::EnsureWinsockInit();
#endif

#if BUILDFLAG(USE_NSS_CERTS)
  // We want to be sure to init NSPR on the main thread.
  crypto::EnsureNSPRInit();
#endif

#if BUILDFLAG(IS_FUCHSIA)
  InitDefaultJob();

  // Have child processes & jobs terminate automatically if the browser process
  // exits, by marking the browser process as "critical" to its job.
  zx_status_t result =
      zx::job::default_job()->set_critical(0, *zx::process::self());
  ZX_CHECK(ZX_OK == result, result) << "zx_job_set_critical";
#endif

  if (parsed_command_line_->HasSwitch(switches::kRendererProcessLimit)) {
    std::string limit_string = parsed_command_line_->GetSwitchValueASCII(
        switches::kRendererProcessLimit);
    size_t process_limit;
    if (base::StringToSizeT(limit_string, &process_limit)) {
      RenderProcessHost::SetMaxRendererProcessCount(process_limit);
    }
  }

  if (parts_)
    parts_->PostEarlyInitialization();

  return RESULT_CODE_NORMAL_EXIT;
}

void BrowserMainLoop::PreCreateMainMessageLoop() {
  TRACE_EVENT0(
      "startup",
      "BrowserMainLoop::CreateMainMessageLoop:PreCreateMainMessageLoop");
  if (parts_) {
    parts_->PreCreateMainMessageLoop();
  }
}

void BrowserMainLoop::CreateMainMessageLoop() {
  // DO NOT add more code here. Use PreCreateMainMessageLoop() above or
  // PostCreateMainMessageLoop() below.

  TRACE_EVENT0("startup", "BrowserMainLoop::CreateMainMessageLoop");

  base::PlatformThread::SetName("CrBrowserMain");

  // Register the main thread. The main thread's task runner should already have
  // been initialized but it's not yet known as BrowserThread::UI.
  DCHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  DCHECK(base::CurrentUIThread::IsSet());
  main_thread_.reset(new BrowserThreadImpl(
      BrowserThread::UI, base::SingleThreadTaskRunner::GetCurrentDefault()));
}

void BrowserMainLoop::PostCreateMainMessageLoop() {
  TRACE_EVENT0("startup", "BrowserMainLoop::PostCreateMainMessageLoop");
  mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics("BrowserMain");
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        mojo::InterfaceEndpointClient::SetThreadNameSuffixForMetrics(
            "BrowserIO");
      }));
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:SystemMonitor");
    system_monitor_ = std::make_unique<base::SystemMonitor>();
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:PowerMonitor");
    if (auto* power_monitor = base::PowerMonitor::GetInstance();
        !power_monitor->IsInitialized()) {
      power_monitor->Initialize(MakePowerMonitorDeviceSource());
    }
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:HighResTimerManager");
    hi_res_timer_manager_ =
        std::make_unique<base::HighResolutionTimerManager>();
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:NetLog");
    if (content::IsOutOfProcessNetworkService()) {
      // Initialize NetLog source IDs to use an alternate starting value for
      // the browser process. This needs to be done early in process startup
      // before any NetLogSource objects might get created.
      net::NetLog::Get()->InitializeSourceIdPartition();
    }
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:NetworkChangeNotifier");
    // On Android if reduced mode started network service this would already be
    //  created.
    network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();
  }
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:ScreenlockMonitor");
    std::unique_ptr<ScreenlockMonitorSource> screenlock_monitor_source =
        std::make_unique<ScreenlockMonitorDeviceSource>();
    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::move(screenlock_monitor_source));
  }
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ContentWebUIController");
    RegisterContentWebUIConfigs();
  }

  {
    TRACE_EVENT0("startup", "BrowserMainLoop::Subsystem:OnlineStateObserver");
    online_state_observer_ = std::make_unique<BrowserOnlineStateObserver>();
  }

  { base::SetRecordActionTaskRunner(GetUIThreadTaskRunner({})); }

  // TODO(boliu): kSingleProcess check is a temporary workaround for
  // in-process Android WebView. crbug.com/503724 tracks proper fix.
  if (!parsed_command_line_->HasSwitch(switches::kSingleProcess)) {
    base::DiscardableMemoryAllocator::SetInstance(
        discardable_memory::DiscardableSharedMemoryManager::Get());
  }

  if (parts_)
    parts_->PostCreateMainMessageLoop();

#if BUILDFLAG(IS_ANDROID)
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ScopedSurfaceRequestManager");
    if (UsingInProcessGpu()) {
      gpu::ScopedSurfaceRequestConduit::SetInstance(
          ScopedSurfaceRequestManager::GetInstance());
    }
  }

  if (!parsed_command_line_->HasSwitch(
          switches::kDisableScreenOrientationLock)) {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:ScreenOrientationProvider");
    screen_orientation_delegate_ =
        std::make_unique<ScreenOrientationDelegateAndroid>();
  }

  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(
      base::trace_event::CPUFreqMonitor::GetInstance());
#endif

  if (UsingInProcessGpu()) {
    // Make sure to limits for skia font cache are applied for in process
    // gpu setup (crbug.com/1183230).
    InitializeSkia();
  } else {
    // Just enable memory-infra dump providers
    InitSkiaEventTracer();
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        skia::SkiaMemoryDumpProvider::GetInstance(), "Skia", nullptr);
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      sql::SqlMemoryDumpProvider::GetInstance(), "Sql", nullptr);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  device::BluetoothAdapterFactory::SetBleScanParserCallback(
      base::BindRepeating(&GetBleScanParser));
#else
  // Chrome Remote Desktop needs TransitionalURLLoaderFactoryOwner on ChromeOS.
  network::TransitionalURLLoaderFactoryOwner::DisallowUsageInProcess();
#endif

  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::Subsystem:BrowserAccessibilityStateImpl");
    browser_accessibility_state_ = BrowserAccessibilityStateImpl::Create();
    browser_accessibility_state_->InitBackgroundTasks();
  }
}

void BrowserMainLoop::CreateMessageLoopForEarlyShutdown() {
  CreateMainMessageLoop();
}

int BrowserMainLoop::PreCreateThreads() {
  TRACE_EVENT0("startup", "BrowserMainLoop::PreCreateThreads");

  // This must occur before metrics recording initialization in
  // ChromeBrowserMainParts::PreCreateThreads() because it's used in
  // BackgroundTracingMetricsProvider.
  tracing_controller_ = std::make_unique<TracingControllerImpl>();
  background_tracing_manager_ =
      BackgroundTracingManagerImpl::CreateInstance();

  // Make sure no accidental call to initialize GpuDataManager earlier.
  DCHECK(!GpuDataManagerImpl::Initialized());
  if (parts_) {
    result_code_ = parts_->PreCreateThreads();
  }

  InitializeMemoryManagementComponent();
#if BUILDFLAG(IS_ANDROID)
  memory_pressure::UserLevelMemoryPressureSignalGenerator::Initialize();
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Prior to any processing happening on the IO thread, we create the
  // plugin service as it is predominantly used from the IO thread,
  // but must be created on the main thread. The service ctor is
  // inexpensive and does not invoke the io_thread() accessor.
  {
    TRACE_EVENT0("startup", "BrowserMainLoop::PluginService");
    PluginService::GetInstance()->Init();
  }
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Prior to any processing happening on the IO thread, we create the
  // CDM service as it is predominantly used from the IO thread. This must
  // be called on the main thread since it involves file path checks.
  CdmRegistry::GetInstance()->Init();
#endif

#if BUILDFLAG(IS_MAC)
  // The WindowResizeHelper allows the UI thread to wait on specific renderer
  // and GPU messages from the IO thread. Initializing it before the IO thread
  // starts ensures the affected IO thread messages always have somewhere to go.
  ui::WindowResizeHelperMac::Get()->Init(
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif

  // GpuDataManager should be initialized in parts_->PreCreateThreads through
  // ChromeBrowserMainExtraPartsGpu. However, if |parts_| is not set,
  // initialize it here.
  // Need to initialize in-process GpuDataManager before creating threads.
  // It's unsafe to append the gpu command line switches to the global
  // CommandLine::ForCurrentProcess object after threads are created.
  GpuDataManagerImpl::GetInstance();
  DCHECK(GpuDataManagerImpl::Initialized());
  // We report Uma metrics on a periodic basis when running the full browser,
  // while avoiding doing so in unit tests by making it explicitly enabled here.
  GpuDataManagerImpl::GetInstance()->StartUmaTimer();

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  // Single-process is an unsupported and not fully tested mode, so
  // don't enable it for official Chrome builds (except on Android and iOS).
  if (parsed_command_line_->HasSwitch(switches::kSingleProcess))
    RenderProcessHost::SetRunRendererInProcess(true);
#endif

  // Initialize origins that require process isolation.  Must be done
  // after base::FeatureList is initialized, but before any navigations can
  // happen.
  SiteIsolationPolicy::ApplyGlobalIsolatedOrigins();

  // Generate the browser process salt. This is then accessible by calls to
  // GetPseudonymizationSalt in the browser process. This generation is only
  // needed in the browser process, because for other processes it is
  // transferred to them over IPC from the relevant process host.
  SetPseudonymizationSalt(GenerateBrowserSalt());

  return result_code_;
}

void BrowserMainLoop::CreateStartupTasks() {
  TRACE_EVENT0("startup", "BrowserMainLoop::CreateStartupTasks");

  DCHECK(!startup_task_runner_);
#if BUILDFLAG(IS_ANDROID)
  // Some java scheduler tests need to test migration to C++, but the browser
  // environment isn't set up fully and if these tasks run they may crash.
  if (!g_post_startup_tasks)
    return;

  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::BindOnce(&BrowserStartupComplete),
      GetUIThreadTaskRunner({BrowserTaskType::kDefault}));
#else
  startup_task_runner_ = std::make_unique<StartupTaskRunner>(
      base::OnceCallback<void(int)>(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif
  StartupTask pre_create_threads = base::BindOnce(
      &BrowserMainLoop::PreCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(pre_create_threads));

  StartupTask create_threads =
      base::BindOnce(&BrowserMainLoop::CreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(create_threads));

  StartupTask post_create_threads = base::BindOnce(
      &BrowserMainLoop::PostCreateThreads, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(post_create_threads));

  StartupTask pre_main_message_loop_run = base::BindOnce(
      &BrowserMainLoop::PreMainMessageLoopRun, base::Unretained(this));
  startup_task_runner_->AddTask(std::move(pre_main_message_loop_run));

// On Android and iOS, the native message loop is already running when the app
// is entered and startup tasks are run asynchrously from it.
// InterceptMainMessageLoopRun() thus needs to be forced instead of happening
// from MainMessageLoopRun().
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  StartupTask intercept_main_message_loop_run = base::BindOnce(
      [](BrowserMainLoop* self) {
        // Lambda to ignore the return value and always keep a clean exit code
        // for this StartupTask.
        self->InterceptMainMessageLoopRun();
        return self->result_code_;
      },
      base::Unretained(this));
  startup_task_runner_->AddTask(std::move(intercept_main_message_loop_run));
#endif

#if BUILDFLAG(IS_ANDROID)
  startup_task_runner_->StartRunningTasksAsync();
#else
  startup_task_runner_->RunAllTasksNow();
#endif
}

scoped_refptr<base::SingleThreadTaskRunner>
BrowserMainLoop::GetResizeTaskRunner() {
#if BUILDFLAG(IS_MAC)
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      ui::WindowResizeHelperMac::Get()->task_runner();
  // In tests, WindowResizeHelperMac task runner might not be initialized.
  return task_runner ? task_runner
                     : base::SingleThreadTaskRunner::GetCurrentDefault();
#else
  return base::SingleThreadTaskRunner::GetCurrentDefault();
#endif
}

gpu::GpuChannelEstablishFactory*
BrowserMainLoop::gpu_channel_establish_factory() const {
  return BrowserGpuChannelHostFactory::instance();
}

#if BUILDFLAG(IS_ANDROID)
void BrowserMainLoop::SynchronouslyFlushStartupTasks() {
  startup_task_runner_->RunAllTasksNow();
}
#endif  // BUILDFLAG(IS_ANDROID)

int BrowserMainLoop::CreateThreads() {
  TRACE_EVENT0("startup,rail", "BrowserMainLoop::CreateThreads");

  // Release the ThreadPool's threads.
  scoped_execution_fence_.reset();

  // The |io_thread| can have optionally been injected into Init(), but if not,
  // create it here. Thre thread is only tagged as BrowserThread::IO here in
  // order to prevent any code from statically posting to it before
  // CreateThreads() (as such maintaining the invariant that PreCreateThreads()
  // et al. "happen-before" BrowserThread::IO is "brought up").
  if (!io_thread_) {
    io_thread_ = BrowserTaskExecutor::CreateIOThread();
  }
  io_thread_->RegisterAsBrowserThread();
  BrowserTaskExecutor::InitializeIOThread();

  // TODO(crbug.com/40584847): Replace with a better API
  GetContentClient()->browser()->PostAfterStartupTask(
      FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](BrowserMainLoop* browser_main_loop) {
            // Informs BrowserTaskExecutor that startup is complete.
            BrowserTaskExecutor::OnStartupComplete();
            browser_main_loop->scoped_best_effort_execution_fence_.reset();
          },
          // Main thread tasks can't run after BrowserMainLoop destruction.
          // Accessing an Unretained pointer to BrowserMainLoop from a main
          // thread task is therefore safe.
          base::Unretained(this)));

  created_threads_ = true;
  return result_code_;
}

int BrowserMainLoop::PostCreateThreads() {
  TRACE_EVENT0("startup", "BrowserMainLoop::PostCreateThreads");

  BackgroundTracingManagerImpl::GetInstance()
      .AddMetadataGeneratorFunction();

  if (parts_)
    parts_->PostCreateThreads();

  PostCreateThreadsImpl();

  return result_code_;
}

int BrowserMainLoop::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "BrowserMainLoop::PreMainMessageLoopRun");

  auto font_render_params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  skia::LegacyDisplayGlobals::SetCachedParams(
      gfx::FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
          font_render_params.subpixel_rendering),
      font_render_params.text_contrast, font_render_params.text_gamma);
  viz::GpuHostImpl::InitFontRenderParams(font_render_params);

#if BUILDFLAG(IS_ANDROID)
  bool use_display_wide_color_gamut =
      GetContentClient()->browser()->GetWideColorGamutHeuristic() ==
      ContentBrowserClient::WideColorGamutHeuristic::kUseDisplay;
  // Let screen instance be overridable by parts.
  ui::SetScreenAndroid(use_display_wide_color_gamut);
#endif  // BUILDFLAG(IS_ANDROID)

  if (parts_)
    result_code_ = parts_->PreMainMessageLoopRun();

  // ShellBrowserMainParts initializes a ShellBrowserContext with user data
  // directory only in PreMainMessageLoopRun(). First-Party Sets handler needs
  // to access this directory, hence triggering after this stage has run.
  if (result_code_ == RESULT_CODE_NORMAL_EXIT) {
    FirstPartySetsHandlerImpl::GetInstance()->Init(
        GetContentClient()->browser()->GetFirstPartySetsDirectory(),
        FirstPartySetParser::ParseFromCommandLine(
            GetRelatedWebsiteSetSwitch()));
  }

  variations::MaybeScheduleFakeCrash();

  // Unretained(this) is safe as the main message loop expected to run it is
  // stopped before ~BrowserMainLoop (in the event the message loop doesn't
  // reach idle before that point).
  idle_callback_subscription_ =
      base::CurrentThread::Get()->RegisterOnNextIdleCallback(
          {}, base::BindOnce(
                  [](BrowserMainLoop* self) {
                    if (self->parts_) {
                      self->parts_->OnFirstIdle();
                    }

                    self->responsiveness_watcher_->OnFirstIdle();

                    // Enable MessagePumpPhases metrics/tracing on-first-idle,
                    // not before as queuing time is not relevant before first
                    // idle.
                    // TODO(crbug.com/40226913): Consider supporting the initial
                    // run (until first idle) as well.
                    auto enable_message_pump_metrics =
                        base::BindRepeating([](const char* thread_name) {
                          base::CurrentThread::Get()
                              ->EnableMessagePumpTimeKeeperMetrics(thread_name);
                        });
                    enable_message_pump_metrics.Run("BrowserUI");
                    GetIOThreadTaskRunner({})->PostTask(
                        FROM_HERE,
                        BindOnce(enable_message_pump_metrics, "BrowserIO"));
                  },
                  base::Unretained(this)));

  // If the UI thread blocks, the whole UI is unresponsive. Do not allow
  // unresponsive tasks from the UI thread and instantiate a
  // responsiveness::Watcher to catch jank induced by any unintentionally
  // blocking tasks.
  base::DisallowUnresponsiveTasks();
  responsiveness_watcher_ = new responsiveness::Watcher;
  responsiveness_watcher_->SetUp();
  return result_code_;
}

BrowserMainLoop::ProceedWithMainMessageLoopRun
BrowserMainLoop::InterceptMainMessageLoopRun() {
  // Embedders can request not to run the loop (also voids |ui_task|).
  if (parts_ && !parts_->ShouldInterceptMainMessageLoopRun())
    return ProceedWithMainMessageLoopRun(false);

  // The |ui_task| can be injected by tests to replace the main message loop.
  if (parameters_.ui_task) {
    std::move(parameters_.ui_task).Run();
    return ProceedWithMainMessageLoopRun(false);
  }

  return ProceedWithMainMessageLoopRun(true);
}

void BrowserMainLoop::RunMainMessageLoop() {
#if BUILDFLAG(IS_ANDROID)
  // Android's main message loop is the Java message loop.
  NOTREACHED_IN_MIGRATION();
#else  // BUILDFLAG(IS_ANDROID)
  if (InterceptMainMessageLoopRun() != ProceedWithMainMessageLoopRun(true))
    return;

  auto main_run_loop = std::make_unique<base::RunLoop>();
  if (parts_)
    parts_->WillRunMainMessageLoop(main_run_loop);

#if BUILDFLAG(IS_MAC)
  // Call Recycle() here as late as possible, before going into the loop because
  // previous steps may have added things to it (e.g. while creating the main
  // window).
  if (parameters_.autorelease_pool)
    parameters_.autorelease_pool->Recycle();
#endif  // BUILDFLAG(IS_MAC)

  DCHECK(main_run_loop);
  main_run_loop->Run();
#endif  // BUILDFLAG(IS_ANDROID)
}

void BrowserMainLoop::PreShutdown() {
#ifdef LEAK_SANITIZER
#if BUILDFLAG(IS_MAC)
  // Ensure autorelease pool is drained before we do the leak check to avoid
  // catching about-to-be-released objects as false positives.
  if (parameters_.autorelease_pool)
    parameters_.autorelease_pool->Recycle();
#endif  // BUILDFLAG(IS_MAC)
  // Invoke leak detection now, to avoid dealing with shutdown-only leaks.
  // Normally this will have already happened in
  // BrowserProcessImpl::Unpin(), so this call has no effect. This is
  // only for processes which do not instantiate a BrowserProcess.
  // If leaks are found, the process will exit here.
  __lsan_do_leak_check();
#endif  // LEAK_SANITIZER

  // Clear OnNextIdleCallback if it's still pending. Failure to do so can result
  // in an OnFirstIdle phase incorrectly triggering during shutdown if an early
  // exit paths results in a shutdown path that happens to RunLoop.
  idle_callback_subscription_ = {};

  ui::Clipboard::OnPreShutdownForCurrentThread();
}

void BrowserMainLoop::ShutdownThreadsAndCleanUp() {
  if (!created_threads_) {
    // Called early, nothing to do
    return;
  }
  TRACE_EVENT0("shutdown", "BrowserMainLoop::ShutdownThreadsAndCleanUp");

  // Teardown may start in PostMainMessageLoopRun, and during teardown we
  // need to be able to perform IO.
  base::PermanentThreadAllowance::AllowBlocking();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(base::IgnoreResult(
                     &base::PermanentThreadAllowance::AllowBlocking)));

  // Also allow waiting to join threads.
  // TODO(crbug.com/40557572): Ideally this (and the above AllowBlocking() would
  // be scoped allowances). That would be one of the first step to ensure no
  // persistent work is being done after ThreadPoolInstance::Shutdown() in order
  // to move towards atomic shutdown.
  base::PermanentThreadAllowance::AllowBaseSyncPrimitives();
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
          &base::PermanentThreadAllowance::AllowBaseSyncPrimitives)));

  if (RenderProcessHost::run_renderer_in_process())
    RenderProcessHostImpl::ShutDownInProcessRenderer();

  base::features::MakeFreeNoOp(
      base::features::WhenFreeBecomesNoOp::kInShutDownThreads);

  if (parts_) {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:PostMainMessageLoopRun");
    parts_->PostMainMessageLoopRun();
  }

  // Request shutdown to clean up allocated resources on the IO thread.
  if (midi_service_) {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:MidiService");
    midi_service_->Shutdown();
  }

  {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:SpeechRecognitionManager");
    io_thread_->task_runner()->DeleteSoon(
        FROM_HERE, speech_recognition_manager_.release());
  }

  TtsControllerImpl::GetInstance()->Shutdown();

  memory_pressure_monitor_.reset();

  ShutDownNetworkService();

  BrowserProcessIOThread::ProcessHostCleanUp();

#if BUILDFLAG(IS_MAC)
  BrowserCompositorMac::DisableRecyclingForShutdown();
#endif

#if defined(USE_AURA)
  if (env_) {
    // ContextFactory is owned by ImageTransportFactory, which will delete the
    // object in the `Terminate` call below. We need to make sure aura::Env
    // doesn't reference it anymore when that happens.
    env_->set_context_factory(nullptr);
  }
#endif

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  {
    TRACE_EVENT0("shutdown",
                 "BrowserMainLoop::Subsystem:ImageTransportFactory");
    ImageTransportFactory::Terminate();
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  host_frame_sink_manager_.reset();
  compositing_mode_reporter_impl_.reset();
#endif

// The device monitors are using |system_monitor_| as dependency, so delete
// them before |system_monitor_| goes away.
// On windows, the monitor needs to be destroyed on the same thread
// as they were created. On Linux, the monitor will be deleted when IO thread
// goes away.
#if BUILDFLAG(IS_WIN)
  system_message_window_.reset();
#endif

  if (BrowserGpuChannelHostFactory::instance())
    BrowserGpuChannelHostFactory::instance()->CloseChannel();

  mojo_ipc_support_.reset();

  if (save_file_manager_)
    save_file_manager_->Shutdown();

  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:IOThread");
    ResetThread_IO(std::move(io_thread_));
  }

  // Must be done before ThreadPool shutdown since the trace report database
  // lives on the ThreadPool.
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem::TracingManager");
    background_tracing_manager_.reset();
  }

  GetContentClient()->browser()->ThreadPoolWillTerminate();
  {
    TRACE_EVENT0("shutdown", "BrowserMainLoop::Subsystem:ThreadPool");
    base::ThreadPoolInstance::Get()->Shutdown();
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
      std::ignore = audio_manager_.release();
      // |user_input_monitor_| may be in use by stray streams in case
      // AudioManager shutdown failed.
      std::ignore = user_input_monitor_.release();
    }

    // Leaking AudioSystem: we cannot correctly destroy it since Audio service
    // connection in there is bound to IO thread.
    std::ignore = audio_system_.release();
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

void BrowserMainLoop::GetCompositingModeReporter(
    mojo::PendingReceiver<viz::mojom::CompositingModeReporter> receiver) {
#if BUILDFLAG(IS_ANDROID)
  // Android doesn't support non-gpu compositing modes, and doesn't make a
  // CompositingModeReporter.
  return;
#else
  compositing_mode_reporter_impl_->BindReceiver(std::move(receiver));
#endif
}

void BrowserMainLoop::PostCreateThreadsImpl() {
  TRACE_EVENT0("startup", "BrowserMainLoop::PostCreateThreadsImpl");

  // Bring up Mojo IPC and the embedded Service Manager as early as possible.
  // Initializaing mojo requires the IO thread to have been initialized first,
  // so this cannot happen any earlier than now.
  InitializeMojo();

  data_decoder_service_provider_ = std::make_unique<OopDataDecoder>();

  HistogramSynchronizer::GetInstance();

  FieldTrialSynchronizer::CreateInstance();

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

  // Initialize the GPU cache. This needs to be initialized before
  // BrowserGpuChannelHostFactory below, since that depends on an initialized
  // GpuDiskCacheFactory.
  InitGpuDiskCacheFactorySingleton();

  bool always_uses_gpu = true;
  bool established_gpu_channel = false;
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40396955): This should be set to |true|.
  established_gpu_channel = false;
  always_uses_gpu = ShouldStartGpuProcessOnBrowserStartup();
  BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
#else
  established_gpu_channel = true;
  if (parsed_command_line_->HasSwitch(switches::kDisableGpu) ||
      parsed_command_line_->HasSwitch(switches::kDisableGpuCompositing) ||
      parsed_command_line_->HasSwitch(switches::kDisableGpuEarlyInit)) {
    established_gpu_channel = always_uses_gpu = false;
  }

  host_frame_sink_manager_ = std::make_unique<viz::HostFrameSinkManager>();
  BrowserGpuChannelHostFactory::Initialize(established_gpu_channel);
  compositing_mode_reporter_impl_ =
      std::make_unique<viz::CompositingModeReporterImpl>();

  auto transport_factory = std::make_unique<VizProcessTransportFactory>(
      BrowserGpuChannelHostFactory::instance(), GetResizeTaskRunner(),
      compositing_mode_reporter_impl_.get());
  transport_factory->ConnectHostFrameSinkManager();
  ImageTransportFactory::SetFactory(std::move(transport_factory));

#if defined(USE_AURA)
  env_->set_context_factory(GetContextFactory());
#endif  // defined(USE_AURA)
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      tracing::GraphicsMemoryDumpProvider::GetInstance(), "AndroidGraphics",
      nullptr);
#endif

  {
    TRACE_EVENT0("startup", "PostCreateThreads::Subsystem:AudioMan");
    InitializeAudio();
  }

  {
    TRACE_EVENT0("startup", "PostCreateThreads::Subsystem:MidiService");
    midi_service_ = std::make_unique<midi::MidiService>();
  }

  {
    TRACE_EVENT0("startup", "PostCreateThreads::Subsystem:Devices");
    device::GamepadService::GetInstance()->StartUp(
        base::BindRepeating(&BindHidManager));
#if !BUILDFLAG(IS_ANDROID)
    device::FidoHidDiscovery::SetHidManagerBinder(
        base::BindRepeating(&BindHidManager));
#endif
  }

#if BUILDFLAG(IS_WIN)
  if (!base::FeatureList::IsEnabled(
          video_capture::features::kWinCameraMonitoringInVideoCaptureService)) {
    system_message_window_ = std::make_unique<media::SystemMessageWindowWin>();
  }
#elif (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(USE_UDEV)
  device_monitor_linux_ = std::make_unique<media::DeviceMonitorLinux>();
#endif

  // Instantiated once using CreateSingletonInstance(), and accessed only using
  // GetInstance(), which is not allowed to create the object. This allows us
  // to ensure that it cannot be used before objects it relies on have been
  // created; namely, WebRtcEventLogManager.
  // Allowed to leak when the browser exits.
  WebRTCInternals::CreateSingletonInstance();

  // MediaStreamManager needs the IO thread to be created.
  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::PostCreateThreads:InitMediaStreamManager");

    media_stream_manager_ =
        std::make_unique<MediaStreamManager>(audio_system_.get());
  }

  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::PostCreateThreads:InitSpeechRecognition");
    speech_recognition_manager_.reset(new SpeechRecognitionManagerImpl(
        audio_system_.get(), media_stream_manager_.get()));
  }

  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::PostCreateThreads::InitUserInputMonitor");
    user_input_monitor_ = media::UserInputMonitor::Create(
        io_thread_->task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  {
    TRACE_EVENT0("startup",
                 "BrowserMainLoop::PostCreateThreads::SaveFileManager");
    save_file_manager_ = new SaveFileManager();
  }

  // Alert the clipboard class to which threads are allowed to access the
  // clipboard:
  std::vector<base::PlatformThreadId> allowed_clipboard_threads;
  // The current thread is the UI thread.
  allowed_clipboard_threads.push_back(base::PlatformThread::CurrentId());
#if BUILDFLAG(IS_WIN)
  // On Windows, clipboard is also used on the IO thread.
  allowed_clipboard_threads.push_back(io_thread_->GetThreadId());
#endif
  ui::Clipboard::SetAllowedThreads(allowed_clipboard_threads);

  if (!established_gpu_channel && always_uses_gpu) {
    TRACE_EVENT_INSTANT0("gpu", "Post task to launch GPU process",
                         TRACE_EVENT_SCOPE_THREAD);
    GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, true /* force_create */);
  }

  GpuDataManagerImpl::GetInstance()->PostCreateThreads();

  if (MediaKeysListenerManager::IsMediaKeysListenerManagerEnabled()) {
    media_keys_listener_manager_ =
        std::make_unique<MediaKeysListenerManagerImpl>();
  }

#if BUILDFLAG(IS_MAC)
  ThemeHelperMac::GetInstance();
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
  media::SetMediaDrmBridgeClient(GetContentClient()->GetMediaDrmBridgeClient());

  // On Android this must be done after SetMediaDrmBridgeClient(). For Android
  // all CDMs are part of the OS, so no file checks are involved.
  CdmRegistry::GetInstance()->Init();

  if (base::FeatureList::IsEnabled(features::kFontSrcLocalMatching)) {
    FontUniqueNameLookup::GetInstance();
  }
#endif

#if defined(ENABLE_IPC_FUZZER)
  SetFileUrlPathAliasForIpcFuzzer();
#endif
}

bool BrowserMainLoop::UsingInProcessGpu() const {
  return parsed_command_line_->HasSwitch(switches::kSingleProcess) ||
         parsed_command_line_->HasSwitch(switches::kInProcessGPU);
}

void BrowserMainLoop::InitializeMemoryManagementComponent() {
  memory_pressure_monitor_ = CreateMemoryPressureMonitor(*parsed_command_line_);
}

bool BrowserMainLoop::InitializeToolkit() {
  TRACE_EVENT0("startup", "BrowserMainLoop::InitializeToolkit");

  // TODO(evan): this function is rather subtle, due to the variety
  // of intersecting ifdefs we have.  To keep it easy to follow, there
  // are no #else branches on any #ifs.
  // TODO(stevenjb): Move platform specific code into platform specific Parts
  // (Need to add InitializeToolkit stage to BrowserParts).
  // See also GTK setup in EarlyInitialization, above, and associated comments.

#if BUILDFLAG(IS_WIN)
  INITCOMMONCONTROLSEX config;
  config.dwSize = sizeof(config);
  config.dwICC = ICC_WIN95_CLASSES;
  if (!InitCommonControlsEx(&config))
    PLOG(FATAL);
#endif

#if defined(USE_AURA)
  // Env creates the compositor. Aura widgets need the compositor to be created
  // before they can be initialized by the browser.
  env_ = aura::Env::CreateInstance();
  if (!env_)
    return false;
#endif  // defined(USE_AURA)

  if (parts_)
    parts_->ToolkitInitialized();

  return true;
}

void BrowserMainLoop::InitializeMojo() {
  if (!parsed_command_line_->HasSwitch(switches::kSingleProcess)) {
    // Disallow mojo sync calls in the browser process. Note that we allow sync
    // calls in single-process mode since renderer IPCs are made from a browser
    // thread.
    mojo::SyncCallRestrictions::DisallowSyncCall();
  }

  // Start startup tracing through TracingController's interface. TraceLog has
  // been enabled in content_main_runner where threads are not available. Now We
  // need to start tracing for all other tracing agents, which require threads.
  // We can only do this after starting the main message loop to avoid calling
  // MessagePumpForUI::ScheduleWork() before MessagePumpForUI::Start().
  StartupTracingController::GetInstance().StartIfNeeded();

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
  mojo::BeginRandomMojoDelays();
#endif
}

void BrowserMainLoop::InitializeAudio() {
  DCHECK(!audio_manager_);

  audio_manager_ = GetContentClient()->browser()->CreateAudioManager(
      MediaInternals::GetInstance());
  DCHECK_EQ(!!audio_manager_,
            GetContentClient()->browser()->OverridesAudioManager());

  // Do not initialize |audio_manager_| if running out of process.
  if (!audio_manager_ &&
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kSingleProcess) ||
       !base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess))) {
    audio_manager_ =
        media::AudioManager::Create(std::make_unique<media::AudioThreadImpl>(),
                                    MediaInternals::GetInstance());
    CHECK(audio_manager_);
  }

  // Iff |audio_manager_| is instantiated, the audio service will run
  // in-process. Complete the setup for that:
  if (audio_manager_) {
    TRACE_EVENT_INSTANT0("startup", "Starting Audio service task runner",
                         TRACE_EVENT_SCOPE_THREAD);
#if BUILDFLAG(IS_MAC)
    // On Mac, the audio task runner must belong to the main thread.
    // See audio_thread_impl.cc and https://crbug.com/158170.
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
#endif
    audio::Service::GetInProcessTaskRunner()->StartWithTaskRunner(
        audio_manager_->GetTaskRunner());
  }

  if (base::FeatureList::IsEnabled(features::kAudioServiceLaunchOnStartup)) {
    // Schedule the audio service startup on the main thread.
    GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE, base::BindOnce([]() {
                     TRACE_EVENT0("audio", "Starting audio service");
                     GetAudioService();
                   }));
  }

  audio_system_ = CreateAudioSystemForAudioService();
  CHECK(audio_system_);
}

bool BrowserMainLoop::AudioServiceOutOfProcess() const {
  // Returns true iff kAudioServiceOutOfProcess feature is enabled and if the
  // embedder does not provide its own in-process AudioManager.
  return base::FeatureList::IsEnabled(features::kAudioServiceOutOfProcess) &&
         !GetContentClient()->browser()->OverridesAudioManager();
}

SmsProvider* BrowserMainLoop::GetSmsProvider() {
  if (!sms_provider_) {
    sms_provider_ = SmsProvider::Create();
  }
  return sms_provider_.get();
}

void BrowserMainLoop::SetSmsProviderForTesting(
    std::unique_ptr<SmsProvider> provider) {
  sms_provider_ = std::move(provider);
}

base::PlatformThreadId BrowserMainLoop::GetIOThreadId() {
  CHECK(io_thread_ && io_thread_->IsRunning());
  return io_thread_->GetThreadId();
}

}  // namespace content
