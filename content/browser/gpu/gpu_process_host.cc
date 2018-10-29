// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/field_trial_recorder.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/in_process_child_thread_params.h"
#include "content/common/service_manager/child_connection.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/runner/common/client_util.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "services/service_manager/sandbox/switches.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "media/mojo/interfaces/android_overlay.mojom.h"
#endif

#if defined(OS_WIN)
#include "sandbox/win/src/sandbox_policy.h"
#include "services/service_manager/sandbox/win/sandbox_win.h"
#include "ui/gfx/win/rendering_window_manager.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_switches.h"  // nogncheck
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

#if defined(OS_MACOSX)
#include "content/browser/gpu/ca_transaction_gpu_coordinator.h"
#endif

namespace content {

base::subtle::Atomic32 GpuProcessHost::gpu_crash_count_ = 0;
bool GpuProcessHost::crashed_before_ = false;
int GpuProcessHost::hardware_accelerated_recent_crash_count_ = 0;
int GpuProcessHost::swiftshader_recent_crash_count_ = 0;
int GpuProcessHost::display_compositor_recent_crash_count_ = 0;

namespace {

// UMA histogram names.
constexpr char kProcessLifetimeEventsHardwareAccelerated[] =
    "GPU.ProcessLifetimeEvents.HardwareAccelerated";
constexpr char kProcessLifetimeEventsSwiftShader[] =
    "GPU.ProcessLifetimeEvents.SwiftShader";
constexpr char kProcessLifetimeEventsDisplayCompositor[] =
    "GPU.ProcessLifetimeEvents.DisplayCompositor";

// Forgive one GPU process crash after this many minutes.
constexpr int kForgiveGpuCrashMinutes = 60;

// Forgive one GPU process crash, when the GPU process is launched to run only
// the display compositor, after this many minutes.
constexpr int kForgiveDisplayCompositorCrashMinutes = 10;

// This matches base::TerminationStatus.
// These values are persisted to logs. Entries (except MAX_ENUM) should not be
// renumbered and numeric values should never be reused. Should also avoid
// OS-defines in this enum to keep the values consistent on all platforms.
enum class GpuTerminationStatus {
  NORMAL_TERMINATION = 0,
  ABNORMAL_TERMINATION = 1,
  PROCESS_WAS_KILLED = 2,
  PROCESS_CRASHED = 3,
  STILL_RUNNING = 4,
  PROCESS_WAS_KILLED_BY_OOM = 5,
  OOM_PROTECTED = 6,
  LAUNCH_FAILED = 7,
  OOM = 8,
  MAX_ENUM = 9,
};

GpuTerminationStatus ConvertToGpuTerminationStatus(
    base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return GpuTerminationStatus::NORMAL_TERMINATION;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return GpuTerminationStatus::ABNORMAL_TERMINATION;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return GpuTerminationStatus::PROCESS_WAS_KILLED;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return GpuTerminationStatus::PROCESS_CRASHED;
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return GpuTerminationStatus::STILL_RUNNING;
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return GpuTerminationStatus::PROCESS_WAS_KILLED_BY_OOM;
#endif
#if defined(OS_ANDROID)
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      return GpuTerminationStatus::OOM_PROTECTED;
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return GpuTerminationStatus::LAUNCH_FAILED;
    case base::TERMINATION_STATUS_OOM:
      return GpuTerminationStatus::OOM;
    case base::TERMINATION_STATUS_MAX_ENUM:
      NOTREACHED();
      return GpuTerminationStatus::MAX_ENUM;
      // Do not add default.
  }
  NOTREACHED();
  return GpuTerminationStatus::ABNORMAL_TERMINATION;
}

// Command-line switches to propagate to the GPU process.
static const char* const kSwitchNames[] = {
    service_manager::switches::kDisableSeccompFilterSandbox,
    service_manager::switches::kGpuSandboxAllowSysVShm,
    service_manager::switches::kGpuSandboxFailuresFatal,
    service_manager::switches::kDisableGpuSandbox,
    service_manager::switches::kNoSandbox,
#if defined(OS_WIN)
    service_manager::switches::kAddGpuAppContainerCaps,
    service_manager::switches::kDisableGpuAppContainer,
    service_manager::switches::kDisableGpuLpac,
    service_manager::switches::kEnableGpuAppContainer,
#endif  // defined(OS_WIN)
    switches::kDisableBreakpad,
    switches::kDisableGpuRasterization,
    switches::kDisableGLExtensions,
    switches::kDisableLogging,
    switches::kDisableShaderNameHashing,
    switches::kDisableSkiaRuntimeOpts,
    switches::kDisableWebRtcHWEncoding,
#if defined(OS_WIN)
    switches::kEnableAcceleratedVpxDecode,
#endif
    switches::kEnableGpuRasterization,
    switches::kEnableLogging,
    switches::kEnableVizDevTools,
    switches::kHeadless,
    switches::kLoggingLevel,
    switches::kEnableLowEndDeviceMode,
    switches::kDisableLowEndDeviceMode,
    switches::kRunAllCompositorStagesBeforeDraw,
    switches::kSkiaFontCacheLimitMb,
    switches::kSkiaResourceCacheLimitMb,
    switches::kTestGLLib,
    switches::kTraceToConsole,
    switches::kUseFakeJpegDecodeAccelerator,
    switches::kUseGpuInTests,
    switches::kV,
    switches::kVModule,
#if defined(OS_MACOSX)
    service_manager::switches::kEnableSandboxLogging,
    switches::kDisableAVFoundationOverlays,
    switches::kDisableMacOverlays,
    switches::kDisableRemoteCoreAnimation,
    switches::kShowMacOverlayBorders,
#endif
#if defined(USE_OZONE)
    switches::kOzonePlatform,
    switches::kDisableExplicitDmaFences,
    switches::kOzoneDumpFile,
#endif
#if defined(USE_X11)
    switches::kX11Display,
#endif
    switches::kGpuBlacklistTestGroup,
    switches::kGpuDriverBugListTestGroup,
    switches::kUseCmdDecoder,
    switches::kForceVideoOverlays,
#if defined(OS_ANDROID)
    switches::kOrderfileMemoryOptimization,
#endif
    switches::kWebglAntialiasingMode,
    switches::kWebglMSAASampleCount,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum GPUProcessLifetimeEvent {
  LAUNCHED = 0,
  // When the GPU process crashes the (DIED_FIRST_TIME + recent_crash_count - 1)
  // bucket in the appropriate UMA histogram will be incremented. The first
  // crash will be DIED_FIRST_TIME, the second DIED_FIRST_TIME+1, etc.
  DIED_FIRST_TIME = 1,
  GPU_PROCESS_LIFETIME_EVENT_MAX = 100,
};

// Indexed by GpuProcessKind. There is one of each kind maximum. This array may
// only be accessed from the IO thread.
GpuProcessHost* g_gpu_process_hosts[GpuProcessHost::GPU_PROCESS_KIND_COUNT];

void RunCallbackOnIO(GpuProcessHost::GpuProcessKind kind,
                     bool force_create,
                     const base::Callback<void(GpuProcessHost*)>& callback) {
  GpuProcessHost* host = GpuProcessHost::Get(kind, force_create);
  callback.Run(host);
}

void OnGpuProcessHostDestroyedOnUI(int host_id, const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      logging::LOG_ERROR, "GpuProcessHostUIShim", message);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif
}

// NOTE: changes to this class need to be reviewed by the security team.
class GpuSandboxedProcessLauncherDelegate
    : public SandboxedProcessLauncherDelegate {
 public:
  explicit GpuSandboxedProcessLauncherDelegate(
      const base::CommandLine& cmd_line)
#if defined(OS_WIN)
      : cmd_line_(cmd_line),
        enable_appcontainer_(true)
#endif
  {
  }

  ~GpuSandboxedProcessLauncherDelegate() override {}

#if defined(OS_WIN)
  bool DisableDefaultPolicy() override {
    return true;
  }

  enum GPUAppContainerEnableState{
      AC_ENABLED = 0, AC_DISABLED_GL = 1, AC_DISABLED_FORCE = 2,
      MAX_ENABLE_STATE = 3,
  };

  bool GetAppContainerId(std::string* appcontainer_id) override {
    if (UseOpenGLRenderer()) {
      base::UmaHistogramEnumeration("GPU.AppContainer.EnableState",
                                    AC_DISABLED_GL, MAX_ENABLE_STATE);
      return false;
    }

    if (!enable_appcontainer_) {
      base::UmaHistogramEnumeration("GPU.AppContainer.EnableState",
                                    AC_DISABLED_FORCE, MAX_ENABLE_STATE);
      return false;
    }

    *appcontainer_id = base::WideToUTF8(cmd_line_.GetProgram().value());
    base::UmaHistogramEnumeration("GPU.AppContainer.EnableState", AC_ENABLED,
                                  MAX_ENABLE_STATE);
    return true;
  }

  // For the GPU process we gotten as far as USER_LIMITED. The next level
  // which is USER_RESTRICTED breaks both the DirectX backend and the OpenGL
  // backend. Note that the GPU process is connected to the interactive
  // desktop.
  bool PreSpawnTarget(sandbox::TargetPolicy* policy) override {
    if (UseOpenGLRenderer()) {
      // Open GL path.
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_UNPROTECTED, 0, policy);
      policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    } else {
      policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                            sandbox::USER_LIMITED);

      // UI restrictions break when we access Windows from outside our job.
      // However, we don't want a proxy window in this process because it can
      // introduce deadlocks where the renderer blocks on the gpu, which in
      // turn blocks on the browser UI thread. So, instead we forgo a window
      // message pump entirely and just add job restrictions to prevent child
      // processes.
      service_manager::SandboxWin::SetJobLevel(
          cmd_line_, sandbox::JOB_LIMITED_USER,
          JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS | JOB_OBJECT_UILIMIT_DESKTOP |
              JOB_OBJECT_UILIMIT_EXITWINDOWS |
              JOB_OBJECT_UILIMIT_DISPLAYSETTINGS,
          policy);
      policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
    }

    // Block this DLL even if it is not loaded by the browser process.
    policy->AddDllToUnload(L"cmsetac.dll");

    if (cmd_line_.HasSwitch(switches::kEnableLogging)) {
      base::string16 log_file_path = logging::GetLogFileFullPath();
      if (!log_file_path.empty()) {
        sandbox::ResultCode result = policy->AddRule(
            sandbox::TargetPolicy::SUBSYS_FILES,
            sandbox::TargetPolicy::FILES_ALLOW_ANY, log_file_path.c_str());
        if (result != sandbox::SBOX_ALL_OK)
          return false;
      }
    }

    return true;
  }

  // TODO: Remove this once AppContainer sandbox is enabled by default.
  void DisableAppContainer() { enable_appcontainer_ = false; }
#endif  // OS_WIN

  service_manager::SandboxType GetSandboxType() override {
#if defined(OS_WIN)
    if (cmd_line_.HasSwitch(service_manager::switches::kDisableGpuSandbox)) {
      DVLOG(1) << "GPU sandbox is disabled";
      return service_manager::SANDBOX_TYPE_NO_SANDBOX;
    }
#endif
    return service_manager::SANDBOX_TYPE_GPU;
  }

 private:
#if defined(OS_WIN)
  bool UseOpenGLRenderer() {
    return cmd_line_.GetSwitchValueASCII(switches::kUseGL) ==
           gl::kGLImplementationDesktopName;
  }

  base::CommandLine cmd_line_;
  bool enable_appcontainer_;
#endif  // OS_WIN
};

#if defined(OS_ANDROID)
template <typename Interface>
void BindJavaInterface(mojo::InterfaceRequest<Interface> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetGlobalJavaInterfaces()->GetInterface(std::move(request));
}
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
void RecordAppContainerStatus(int error_code, bool crashed_before) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!crashed_before &&
      service_manager::SandboxWin::IsAppContainerEnabledForSandbox(
          *command_line, service_manager::SANDBOX_TYPE_GPU)) {
    base::UmaHistogramSparse("GPU.AppContainer.Status", error_code);
  }
}
#endif  // defined(OS_WIN)

void BindDiscardableMemoryRequestOnIO(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request,
    discardable_memory::DiscardableSharedMemoryManager* manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_manager::BindSourceInfo source_info;
  manager->Bind(std::move(request), source_info);
}

void BindDiscardableMemoryRequestOnUI(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(USE_AURA)
  if (features::IsMultiProcessMash()) {
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        ws::mojom::kServiceName, std::move(request));
    return;
  }
#endif
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BindDiscardableMemoryRequestOnIO, std::move(request),
          BrowserMainLoop::GetInstance()->discardable_shared_memory_manager()));
}

}  // anonymous namespace

class GpuProcessHost::ConnectionFilterImpl : public ConnectionFilter {
 public:
  explicit ConnectionFilterImpl(int gpu_process_id) {
    auto task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
    registry_.AddInterface(base::Bind(&FieldTrialRecorder::Create),
                           task_runner);
#if defined(OS_ANDROID)
    registry_.AddInterface(
        base::Bind(&BindJavaInterface<media::mojom::AndroidOverlayProvider>),
        task_runner);
#endif
  }

 private:
  // ConnectionFilter:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle* interface_pipe,
                       service_manager::Connector* connector) override {
    if (!registry_.TryBindInterface(interface_name, interface_pipe)) {
      GetContentClient()->browser()->BindInterfaceRequest(
          source_info, interface_name, interface_pipe);
    }
  }

  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFilterImpl);
};

// static
bool GpuProcessHost::ValidateHost(GpuProcessHost* host) {
  // The Gpu process is invalid if it's not using SwiftShader, the card is
  // blacklisted, and we can kill it and start over.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU) ||
      host->valid_) {
    return true;
  }

  host->ForceShutdown();
  return false;
}

// static
GpuProcessHost* GpuProcessHost::Get(GpuProcessKind kind, bool force_create) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't grant further access to GPU if it is not allowed.
  GpuDataManagerImpl* gpu_data_manager = GpuDataManagerImpl::GetInstance();
  DCHECK(gpu_data_manager);
  if (!gpu_data_manager->GpuProcessStartAllowed()) {
    DLOG(ERROR) << "!GpuDataManagerImpl::GpuProcessStartAllowed()";
    return nullptr;
  }

  if (g_gpu_process_hosts[kind] && ValidateHost(g_gpu_process_hosts[kind]))
    return g_gpu_process_hosts[kind];

  if (!force_create)
    return nullptr;

  // Do not create a new process if browser is shutting down.
  if (BrowserMainRunner::ExitedMainMessageLoop()) {
    DLOG(ERROR) << "BrowserMainRunner::ExitedMainMessageLoop()";
    return nullptr;
  }

  static int last_host_id = 0;
  int host_id;
  host_id = ++last_host_id;

  GpuProcessHost* host = new GpuProcessHost(host_id, kind);
  if (host->Init())
    return host;

  // TODO(sievers): Revisit this behavior. It's not really a crash, but we also
  // want the fallback-to-sw behavior if we cannot initialize the GPU.
  host->RecordProcessCrash();

  delete host;
  DLOG(ERROR) << "GpuProcessHost::Init() failed";
  return nullptr;
}

// static
void GpuProcessHost::GetHasGpuProcess(base::OnceCallback<void(bool)> callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&GpuProcessHost::GetHasGpuProcess, std::move(callback)));
    return;
  }
  bool has_gpu = false;
  for (size_t i = 0; i < arraysize(g_gpu_process_hosts); ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && ValidateHost(host)) {
      has_gpu = true;
      break;
    }
  }
  std::move(callback).Run(has_gpu);
}

// static
void GpuProcessHost::CallOnIO(
    GpuProcessKind kind,
    bool force_create,
    const base::Callback<void(GpuProcessHost*)>& callback) {
#if !defined(OS_WIN)
  DCHECK_NE(kind, GpuProcessHost::GPU_PROCESS_KIND_UNSANDBOXED_NO_GL);
#endif
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&RunCallbackOnIO, kind, force_create, callback));
}

void GpuProcessHost::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (interface_name ==
      discardable_memory::mojom::DiscardableSharedMemoryManager::Name_) {
    BindDiscardableMemoryRequest(
        discardable_memory::mojom::DiscardableSharedMemoryManagerRequest(
            std::move(interface_pipe)));
    return;
  }
  process_->child_connection()->BindInterface(interface_name,
                                              std::move(interface_pipe));
}

#if defined(USE_OZONE)
void GpuProcessHost::TerminateGpuProcess(const std::string& message) {
  // At the moment, this path is only used by Ozone/Wayland. Once others start
  // to use this, start to distinguish the origin of termination. By default,
  // it's unknown.
  termination_origin_ = GpuTerminationOrigin::kOzoneWaylandProxy;
  process_->TerminateOnBadMessageReceived(message);
}

void GpuProcessHost::SendGpuProcessMessage(IPC::Message* message) {
  Send(message);
}
#endif  // defined(USE_OZONE)

// static
GpuProcessHost* GpuProcessHost::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (int i = 0; i < GPU_PROCESS_KIND_COUNT; ++i) {
    GpuProcessHost* host = g_gpu_process_hosts[i];
    if (host && host->host_id_ == host_id && ValidateHost(host))
      return host;
  }

  return nullptr;
}

// static
int GpuProcessHost::GetGpuCrashCount() {
  return static_cast<int>(base::subtle::NoBarrier_Load(&gpu_crash_count_));
}

// static
void GpuProcessHost::IncrementCrashCount(int forgive_minutes,
                                         int* crash_count) {
  DCHECK_GT(forgive_minutes, 0);

  // Last time the process crashed.
  static base::TimeTicks last_crash_time;

  // Remove one crash per |forgive_minutes| from the crash count, so occasional
  // crashes won't add up and eventually prevent using the GPU process.
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (crashed_before_) {
    int minutes_delta = (current_time - last_crash_time).InMinutes();
    int crashes_to_forgive = minutes_delta / forgive_minutes;
    *crash_count = std::max(0, *crash_count - crashes_to_forgive);
  }
  ++(*crash_count);

  crashed_before_ = true;
  last_crash_time = current_time;
}

GpuProcessHost::GpuProcessHost(int host_id, GpuProcessKind kind)
    : host_id_(host_id),
      valid_(true),
      in_process_(false),
      kind_(kind),
      process_launched_(false),
      connection_filter_id_(
          ServiceManagerConnection::kInvalidConnectionFilterId),
      weak_ptr_factory_(this) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    in_process_ = true;
  }

  // If the 'single GPU process' policy ever changes, we still want to maintain
  // it for 'gpu thread' mode and only create one instance of host and thread.
  DCHECK(!in_process_ || g_gpu_process_hosts[kind] == nullptr);

  g_gpu_process_hosts[kind] = this;

  process_.reset(new BrowserChildProcessHostImpl(
      PROCESS_TYPE_GPU, this, mojom::kGpuServiceName));
}

GpuProcessHost::~GpuProcessHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (in_process_gpu_thread_)
    DCHECK(process_);

  SendOutstandingReplies();

#if defined(OS_MACOSX)
  if (ca_transaction_gpu_coordinator_) {
    ca_transaction_gpu_coordinator_->HostWillBeDestroyed();
    ca_transaction_gpu_coordinator_ = nullptr;
  }
#endif

  // In case we never started, clean up.
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }

  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = nullptr;

#if defined(OS_ANDROID)
  UMA_HISTOGRAM_COUNTS_100("GPU.AtExitSurfaceCount",
                           gpu::GpuSurfaceTracker::Get()->GetSurfaceCount());
#endif

  std::string message;
  bool block_offscreen_contexts = true;
  if (!in_process_ && process_launched_ &&
      kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    ChildProcessTerminationInfo info =
        process_->GetTerminationInfo(false /* known_dead */);
    UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationStatus2",
                              ConvertToGpuTerminationStatus(info.status),
                              GpuTerminationStatus::MAX_ENUM);

    if (info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
        info.status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
        info.status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
      // Windows always returns PROCESS_CRASHED on abnormal termination, as it
      // doesn't have a way to distinguish the two.
      base::UmaHistogramSparse("GPU.GPUProcessExitCode",
                               std::max(0, std::min(100, info.exit_code)));
    }

    switch (info.status) {
      case base::TERMINATION_STATUS_NORMAL_TERMINATION:
        // Don't block offscreen contexts (and force page reload for webgl)
        // if this was an intentional shutdown or the OOM killer on Android
        // killed us while Chrome was in the background.
// TODO(crbug.com/598400): Restrict this to Android for now, since other
// platforms might fall through here for the 'exit_on_context_lost' workaround.
#if defined(OS_ANDROID)
        block_offscreen_contexts = false;
#endif
        message = "The GPU process exited normally. Everything is okay.";
        break;
      case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
        message = base::StringPrintf("The GPU process exited with code %d.",
                                     info.exit_code);
        break;
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
        UMA_HISTOGRAM_ENUMERATION("GPU.GPUProcessTerminationOrigin",
                                  termination_origin_,
                                  GpuTerminationOrigin::kMax);
        message = "You killed the GPU process! Why?";
        break;
#if defined(OS_CHROMEOS)
      case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
        message = "The GUP process was killed due to out of memory.";
        break;
#endif
      case base::TERMINATION_STATUS_PROCESS_CRASHED:
        message = "The GPU process crashed!";
        break;
      case base::TERMINATION_STATUS_LAUNCH_FAILED:
        message = "The GPU process failed to start!";
        break;
      default:
        break;
    }
  }

  // If there are any remaining offscreen contexts at the point the GPU process
  // exits, assume something went wrong, and block their URLs from accessing
  // client 3D APIs without prompting.
  if (block_offscreen_contexts && gpu_host_)
    gpu_host_->BlockLiveOffscreenContexts();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&OnGpuProcessHostDestroyedOnUI, host_id_, message));

  if (ServiceManagerConnection::GetForProcess()) {
    ServiceManagerConnection::GetForProcess()->RemoveConnectionFilter(
        connection_filter_id_);
  }
}

bool GpuProcessHost::Init() {
  init_start_time_ = base::TimeTicks::Now();

  TRACE_EVENT_INSTANT0("gpu", "LaunchGpuProcess", TRACE_EVENT_SCOPE_THREAD);

  // May be null during test execution.
  if (ServiceManagerConnection::GetForProcess()) {
    connection_filter_id_ =
        ServiceManagerConnection::GetForProcess()->AddConnectionFilter(
            std::make_unique<ConnectionFilterImpl>(process_->GetData().id));
  }

  process_->GetHost()->CreateChannelMojo();

  mode_ = GpuDataManagerImpl::GetInstance()->GetGpuMode();
  DCHECK_NE(mode_, gpu::GpuMode::DISABLED);

  if (in_process_) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(GetGpuMainThreadFactory());
    gpu::GpuPreferences gpu_preferences = GetGpuPreferencesFromCommandLine();
    GpuDataManagerImpl::GetInstance()->UpdateGpuPreferences(&gpu_preferences);
    in_process_gpu_thread_.reset(GetGpuMainThreadFactory()(
        InProcessChildThreadParams(
            base::ThreadTaskRunnerHandle::Get(),
            process_->GetInProcessMojoInvitation(),
            process_->child_connection()->service_token()),
        gpu_preferences));
    base::Thread::Options options;
#if defined(OS_WIN) || defined(OS_MACOSX)
    // WGL needs to create its own window and pump messages on it.
    options.message_loop_type = base::MessageLoop::TYPE_UI;
#endif
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    options.priority = base::ThreadPriority::DISPLAY;
#endif
    in_process_gpu_thread_->StartWithOptions(options);

    OnProcessLaunched();  // Fake a callback that the process is ready.
  } else if (!LaunchGpuProcess()) {
    return false;
  }

  viz::mojom::VizMainAssociatedPtr viz_main_ptr;
  process_->child_channel()
      ->GetAssociatedInterfaceSupport()
      ->GetRemoteAssociatedInterface(&viz_main_ptr);
  viz::GpuHostImpl::InitParams params;
  params.restart_id = host_id_;
  params.in_process = in_process_;
  params.disable_gpu_shader_disk_cache =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache);
  params.product = GetContentClient()->GetProduct();
  params.deadline_to_synchronize_surfaces =
      switches::GetDeadlineToSynchronizeSurfaces();
  params.main_thread_task_runner =
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI});
  gpu_host_ = std::make_unique<viz::GpuHostImpl>(
      this, std::make_unique<viz::VizMainWrapper>(std::move(viz_main_ptr)),
      std::move(params));

#if defined(OS_MACOSX)
  ca_transaction_gpu_coordinator_ = CATransactionGPUCoordinator::Create(this);
#endif

  return true;
}

bool GpuProcessHost::Send(IPC::Message* msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_->GetHost()->IsChannelOpening()) {
    queued_messages_.push(msg);
    return true;
  }

  bool result = process_->Send(msg);
  if (!result) {
    // Channel is hosed, but we may not get destroyed for a while. Send
    // outstanding channel creation failures now so that the caller can restart
    // with a new process/channel without waiting.
    SendOutstandingReplies();
  }
  return result;
}

bool GpuProcessHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnMessageReceived(message);
#endif
  return true;
}

void GpuProcessHost::OnChannelConnected(int32_t peer_pid) {
  TRACE_EVENT0("gpu", "GpuProcessHost::OnChannelConnected");

  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}


void GpuProcessHost::OnProcessLaunched() {
  UMA_HISTOGRAM_TIMES("GPU.GPUProcessLaunchTime",
                      base::TimeTicks::Now() - init_start_time_);
#if defined(OS_WIN)
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    RecordAppContainerStatus(sandbox::SBOX_ALL_OK, crashed_before_);
#endif  // defined(OS_WIN)

  if (!in_process_) {
    process_id_ = process_->GetProcess().Pid();
    DCHECK_NE(base::kNullProcessId, process_id_);
    gpu_host_->OnProcessLaunched(process_id_);
  }
}

void GpuProcessHost::OnProcessLaunchFailed(int error_code) {
#if defined(OS_WIN)
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    RecordAppContainerStatus(error_code, crashed_before_);
#endif  // defined(OS_WIN)
  RecordProcessCrash();
}

void GpuProcessHost::OnProcessCrashed(int exit_code) {
  // Record crash before doing anything that could start a new GPU process.
  RecordProcessCrash();

  gpu_host_->OnProcessCrashed();

  SendOutstandingReplies();

  ChildProcessTerminationInfo info =
      process_->GetTerminationInfo(true /* known_dead */);
  GpuDataManagerImpl::GetInstance()->ProcessCrashed(info.status);
}

gpu::GPUInfo GpuProcessHost::GetGPUInfo() const {
  return GpuDataManagerImpl::GetInstance()->GetGPUInfo();
}

gpu::GpuFeatureInfo GpuProcessHost::GetGpuFeatureInfo() const {
  return GpuDataManagerImpl::GetInstance()->GetGpuFeatureInfo();
}

void GpuProcessHost::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  if (kind_ != GPU_PROCESS_KIND_UNSANDBOXED_NO_GL) {
    auto* gpu_data_manager = GpuDataManagerImpl::GetInstance();
    // Update GpuFeatureInfo first, because UpdateGpuInfo() will notify all
    // listeners.
    gpu_data_manager->UpdateGpuFeatureInfo(gpu_feature_info,
                                           gpu_feature_info_for_hardware_gpu);
    gpu_data_manager->UpdateGpuInfo(gpu_info, gpu_info_for_hardware_gpu);
  }
}

void GpuProcessHost::DidFailInitialize() {
  if (kind_ == GPU_PROCESS_KIND_SANDBOXED)
    GpuDataManagerImpl::GetInstance()->FallBackToNextGpuMode();
}

void GpuProcessHost::DidCreateContextSuccessfully() {
#if defined(OS_ANDROID)
  // Android may kill the GPU process to free memory, especially when the app
  // is the background, so Android cannot have a hard limit on GPU starts.
  // Reset crash count on Android when context creation succeeds.
  hardware_accelerated_recent_crash_count_ = 0;
#endif
}

void GpuProcessHost::BlockDomainFrom3DAPIs(const GURL& url,
                                           gpu::DomainGuilt guilt) {
  GpuDataManagerImpl::GetInstance()->BlockDomainFrom3DAPIs(url, guilt);
}

bool GpuProcessHost::GpuAccessAllowed() const {
  return GpuDataManagerImpl::GetInstance()->GpuAccessAllowed(nullptr);
}

void GpuProcessHost::DisableGpuCompositing() {
#if !defined(OS_ANDROID)
  // TODO(crbug.com/819474): The switch from GPU to software compositing should
  // be handled here instead of by ImageTransportFactory.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI}, base::BindOnce([]() {
        if (auto* factory = ImageTransportFactory::GetInstance())
          factory->DisableGpuCompositing();
      }));
#endif
}

gpu::ShaderCacheFactory* GpuProcessHost::GetShaderCacheFactory() {
  return GetShaderCacheFactorySingleton();
}

void GpuProcessHost::RecordLogMessage(int32_t severity,
                                      const std::string& header,
                                      const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(severity, header, message);
}

void GpuProcessHost::BindDiscardableMemoryRequest(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&BindDiscardableMemoryRequestOnUI, std::move(request)));
}

GpuProcessHost::GpuProcessKind GpuProcessHost::kind() {
  return kind_;
}

void GpuProcessHost::ForceShutdown() {
  // This is only called on the IO thread so no race against the constructor
  // for another GpuProcessHost.
  if (g_gpu_process_hosts[kind_] == this)
    g_gpu_process_hosts[kind_] = nullptr;

  process_->ForceShutdown();
}

bool GpuProcessHost::LaunchGpuProcess() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  base::CommandLine::StringType gpu_launcher =
      browser_command_line.GetSwitchValueNative(switches::kGpuLauncher);

#if defined(OS_ANDROID)
  // crbug.com/447735. readlink("self/proc/exe") sometimes fails on Android
  // at startup with EACCES. As a workaround ignore this here, since the
  // executable name is actually not used or useful anyways.
  std::unique_ptr<base::CommandLine> cmd_line =
      std::make_unique<base::CommandLine>(base::CommandLine::NO_PROGRAM);
#else
#if defined(OS_LINUX)
  int child_flags = gpu_launcher.empty() ? ChildProcessHost::CHILD_ALLOW_SELF :
                                           ChildProcessHost::CHILD_NORMAL;
#else
  int child_flags = ChildProcessHost::CHILD_NORMAL;
#endif

  base::FilePath exe_path = ChildProcessHost::GetChildPath(child_flags);
  if (exe_path.empty())
    return false;

  std::unique_ptr<base::CommandLine> cmd_line =
      std::make_unique<base::CommandLine>(exe_path);
#endif

  cmd_line->AppendSwitchASCII(switches::kProcessType, switches::kGpuProcess);

  BrowserChildProcessHostImpl::CopyFeatureAndFieldTrialFlags(cmd_line.get());
  BrowserChildProcessHostImpl::CopyTraceStartupFlags(cmd_line.get());

#if defined(OS_WIN)
  cmd_line->AppendArg(switches::kPrefetchArgumentGpu);
#endif  // defined(OS_WIN)

  if (kind_ == GPU_PROCESS_KIND_UNSANDBOXED_NO_GL) {
    cmd_line->AppendSwitch(service_manager::switches::kDisableGpuSandbox);
    cmd_line->AppendSwitchASCII(switches::kUseGL,
                                gl::kGLImplementationDisabledName);
  }

  // TODO(penghuang): Replace all GPU related switches with GpuPreferences.
  // https://crbug.com/590825
  // If you want a browser command-line switch passed to the GPU process
  // you need to add it to |kSwitchNames| at the beginning of this file.
  cmd_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                             arraysize(kSwitchNames));
  cmd_line->CopySwitchesFrom(
      browser_command_line, switches::kGLSwitchesCopiedFromGpuProcessHost,
      switches::kGLSwitchesCopiedFromGpuProcessHostNumSwitches);

  if (browser_command_line.HasSwitch(switches::kDisableFrameRateLimit))
    cmd_line->AppendSwitch(switches::kDisableGpuVsync);

  std::vector<const char*> gpu_workarounds;
  gpu::GpuDriverBugList::AppendAllWorkarounds(&gpu_workarounds);
  cmd_line->CopySwitchesFrom(browser_command_line, gpu_workarounds.data(),
                             gpu_workarounds.size());

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(
      cmd_line.get(), process_->GetData().id);

  // TODO(kylechar): The command line flags added here should be based on
  // |mode_|.
  GpuDataManagerImpl::GetInstance()->AppendGpuCommandLine(cmd_line.get());
  bool swiftshader_rendering =
      (cmd_line->GetSwitchValueASCII(switches::kUseGL) ==
       gl::kGLImplementationSwiftShaderForWebGLName);

  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessSoftwareRendering",
                        swiftshader_rendering);

  // If specified, prepend a launcher program to the command line.
  if (!gpu_launcher.empty())
    cmd_line->PrependWrapper(gpu_launcher);

  std::unique_ptr<GpuSandboxedProcessLauncherDelegate> delegate =
      std::make_unique<GpuSandboxedProcessLauncherDelegate>(*cmd_line);
#if defined(OS_WIN)
  if (crashed_before_)
    delegate->DisableAppContainer();
#endif  // defined(OS_WIN)
  process_->Launch(std::move(delegate), std::move(cmd_line), true);
  process_launched_ = true;

  if (kind_ == GPU_PROCESS_KIND_SANDBOXED) {
    if (mode_ == gpu::GpuMode::HARDWARE_ACCELERATED) {
      UMA_HISTOGRAM_ENUMERATION(kProcessLifetimeEventsHardwareAccelerated,
                                LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
    } else if (mode_ == gpu::GpuMode::SWIFTSHADER) {
      UMA_HISTOGRAM_ENUMERATION(kProcessLifetimeEventsSwiftShader, LAUNCHED,
                                GPU_PROCESS_LIFETIME_EVENT_MAX);
    } else if (mode_ == gpu::GpuMode::DISPLAY_COMPOSITOR) {
      UMA_HISTOGRAM_ENUMERATION(kProcessLifetimeEventsDisplayCompositor,
                                LAUNCHED, GPU_PROCESS_LIFETIME_EVENT_MAX);
    }
  }

  return true;
}

void GpuProcessHost::SendOutstandingReplies() {
  valid_ = false;

  if (gpu_host_)
    gpu_host_->SendOutstandingReplies();
}

void GpuProcessHost::RecordProcessCrash() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Maximum number of times the GPU process can crash before we try something
  // different, like disabling hardware acceleration or all GL.
  constexpr int kGpuFallbackCrashCount = 3;
#else
  // Android and Chrome OS switch to software compositing and fallback crashes
  // the browser process. For Android the OS can also kill the GPU process
  // arbitrarily. Use a larger maximum crash count here.
  constexpr int kGpuFallbackCrashCount = 6;
#endif

  // Ending only acts as a failure if the GPU process was actually started and
  // was intended for actual rendering (and not just checking caps or other
  // options).
  if (!process_launched_ || kind_ != GPU_PROCESS_KIND_SANDBOXED)
    return;

  // Keep track of the total number of GPU crashes.
  base::subtle::NoBarrier_AtomicIncrement(&gpu_crash_count_, 1);

  int recent_crash_count = 0;
  if (mode_ == gpu::GpuMode::HARDWARE_ACCELERATED) {
    IncrementCrashCount(kForgiveGpuCrashMinutes,
                        &hardware_accelerated_recent_crash_count_);
    UMA_HISTOGRAM_EXACT_LINEAR(
        kProcessLifetimeEventsHardwareAccelerated,
        DIED_FIRST_TIME + hardware_accelerated_recent_crash_count_ - 1,
        static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));
    recent_crash_count = hardware_accelerated_recent_crash_count_;
  } else if (mode_ == gpu::GpuMode::SWIFTSHADER) {
    IncrementCrashCount(kForgiveGpuCrashMinutes,
                        &swiftshader_recent_crash_count_);
    UMA_HISTOGRAM_EXACT_LINEAR(
        kProcessLifetimeEventsSwiftShader,
        DIED_FIRST_TIME + swiftshader_recent_crash_count_ - 1,
        static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));
    recent_crash_count = swiftshader_recent_crash_count_;
  } else if (mode_ == gpu::GpuMode::DISPLAY_COMPOSITOR) {
    IncrementCrashCount(kForgiveDisplayCompositorCrashMinutes,
                        &display_compositor_recent_crash_count_);
    UMA_HISTOGRAM_EXACT_LINEAR(
        kProcessLifetimeEventsDisplayCompositor,
        DIED_FIRST_TIME + display_compositor_recent_crash_count_ - 1,
        static_cast<int>(GPU_PROCESS_LIFETIME_EVENT_MAX));
    recent_crash_count = display_compositor_recent_crash_count_;
  }

  // GPU process initialization failed and fallback already happened.
  if (!gpu_host_ || !gpu_host_->initialized())
    return;

  bool disable_crash_limit = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableGpuProcessCrashLimit);

  // GPU process crashed too many times, fallback on a different GPU process
  // mode.
  if (recent_crash_count >= kGpuFallbackCrashCount && !disable_crash_limit)
    GpuDataManagerImpl::GetInstance()->FallBackToNextGpuMode();
}

viz::mojom::GpuService* GpuProcessHost::gpu_service() {
  DCHECK(gpu_host_);
  return gpu_host_->gpu_service();
}

int GpuProcessHost::GetIDForTesting() const {
  return process_->GetData().id;
}

}  // namespace content
