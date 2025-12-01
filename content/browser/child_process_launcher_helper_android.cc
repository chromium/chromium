// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher_helper.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/apk_assets.h"
#include "base/android/apk_info.h"
#include "base/android/application_status_listener.h"
#include "base/android/jni_array.h"
#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/launch.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/android/spare_renderer_priority.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/memory_pressure/user_level_memory_pressure_signal_generator.h"
#include "content/browser/posix_file_descriptor_info_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ChildProcessLauncherHelperImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace content {
namespace internal {
namespace {

// Controls whether to explicitly enable service group importance logic.
BASE_FEATURE(kServiceGroupImportance, base::FEATURE_ENABLED_BY_DEFAULT);

// Stops a child process based on the handle returned from StartChildProcess.
void StopChildProcess(base::ProcessHandle handle) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  Java_ChildProcessLauncherHelperImpl_stop(env, static_cast<jint>(handle));
}

}  // namespace

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {}

std::optional<mojo::NamedPlatformChannel>
ChildProcessLauncherHelper::CreateNamedPlatformChannelOnLauncherThread() {
  return std::nullopt;
}

std::unique_ptr<PosixFileDescriptorInfo>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());

  // Android WebView runs in single process, ensure that we never get here when
  // running in single process mode.
  CHECK(!command_line()->HasSwitch(switches::kSingleProcess));

  std::unique_ptr<PosixFileDescriptorInfo> files_to_register =
      CreateDefaultPosixFilesToMap(
          child_process_id(), mojo_channel_->remote_endpoint(),
          file_data_->files_to_preload, GetProcessType(), command_line());

#if ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE
  base::MemoryMappedFile::Region icu_region;
  int fd = base::i18n::GetIcuDataFileHandle(&icu_region);
  files_to_register->ShareWithRegion(kAndroidICUDataDescriptor, fd, icu_region);
#endif  // ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE

  return files_to_register;
}

bool ChildProcessLauncherHelper::IsUsingLaunchOptions() {
  return false;
}

bool ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    PosixFileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  DCHECK(!options);

  // Android only supports renderer, sandboxed utility and gpu.
  std::string process_type =
      command_line()->GetSwitchValueASCII(switches::kProcessType);
  CHECK(process_type == switches::kGpuProcess ||
        process_type == switches::kRendererProcess ||
        process_type == switches::kUtilityProcess)
      << "Unsupported process type: " << process_type;

  // Non-sandboxed utility or renderer process are currently not supported.
  DCHECK(process_type == switches::kGpuProcess ||
         !command_line()->HasSwitch(sandbox::policy::switches::kNoSandbox));

  // The child processes can't correctly retrieve host package information so we
  // rather feed this information through the command line.
  command_line()->AppendSwitchASCII(
      switches::kHostPackageName, base::android::apk_info::host_package_name());
  command_line()->AppendSwitchASCII(switches::kPackageName,
                                    base::android::apk_info::package_name());
  command_line()->AppendSwitchASCII(
      switches::kHostPackageLabel,
      base::android::apk_info::host_package_label());
  command_line()->AppendSwitchASCII(
      switches::kHostVersionCode, base::android::apk_info::host_version_code());
  command_line()->AppendSwitchASCII(
      switches::kPackageVersionName,
      base::android::apk_info::package_version_name());

  return true;
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions* options,
    std::unique_ptr<PosixFileDescriptorInfo> files_to_register,
    bool can_use_warm_up_connection,
    bool is_spare_renderer,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK(!options);
  *is_synchronous_launch = false;

  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);

  // Create the Command line String[]
  ScopedJavaLocalRef<jobjectArray> j_argv =
      ToJavaArrayOfStrings(env, command_line()->argv());

  size_t file_count = files_to_register->GetMappingSize();
  DCHECK(file_count > 0);

  std::vector<int32_t> ids(file_count);
  std::vector<int32_t> fds(file_count);
  std::vector<bool> auto_closes(file_count);
  std::vector<int64_t> offsets(file_count);
  std::vector<int64_t> sizes(file_count);
  for (size_t i = 0; i < file_count; ++i) {
    int fd = files_to_register->GetFDAt(i);
    CHECK(0 <= fd);
    fds[i] = fd;
    ids[i] = files_to_register->GetIDAt(i);
    const auto& region = files_to_register->GetRegionAt(i);
    offsets[i] = region.offset;
    sizes[i] = region.size;
    bool auto_close = files_to_register->OwnsFD(fd);
    if (auto_close) {
      std::ignore = files_to_register->ReleaseFD(fd).release();
    }
    auto_closes[i] = auto_close;
  }

  ScopedJavaLocalRef<jobjectArray> j_file_infos =
      Java_ChildProcessLauncherHelperImpl_makeFdInfos(
          env, ids, fds, auto_closes, offsets, sizes);
  CHECK(j_file_infos.obj());

  AddRef();  // Balanced by OnChildProcessStarted.
  java_peer_.Reset(Java_ChildProcessLauncherHelperImpl_createAndStart(
      env, reinterpret_cast<intptr_t>(this), j_argv, j_file_infos,
      can_use_warm_up_connection, is_spare_renderer));

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::set_java_peer_available_on_client_thread,
          this));

  return Process();
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions* options) {
  // Reset any FDs still held open.
  file_data_.reset();
}

ChildProcessTerminationInfo ChildProcessLauncherHelper::GetTerminationInfo(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead) {
  ChildProcessTerminationInfo info;
  if (!java_peer_avaiable_on_client_thread_)
    return info;

  Java_ChildProcessLauncherHelperImpl_getTerminationInfoAndStop(
      AttachCurrentThread(), java_peer_, reinterpret_cast<intptr_t>(&info));

  base::android::ApplicationState app_state =
      base::android::ApplicationStatusListener::GetState();
  bool app_foreground =
      app_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES ||
      app_state == base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES;

  if (app_foreground &&
      (info.binding_state == base::android::ChildBindingState::VISIBLE ||
       info.binding_state == base::android::ChildBindingState::STRONG)) {
    info.status = base::TERMINATION_STATUS_OOM_PROTECTED;
  } else {
    // Note waitpid does not work on Android since these are not actually child
    // processes. So there is no need for base::GetTerminationInfo.
    info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
  }
  info.memory_pressure_metrics =
      UserLevelMemoryPressureSignalGenerator::GetLatestMemoryMetrics();
  return info;
}

static void JNI_ChildProcessLauncherHelperImpl_SetTerminationInfo(
    JNIEnv* env,
    jlong termination_info_ptr,
    jint binding_state,
    jboolean killed_by_us,
    jboolean clean_exit,
    jboolean exception_during_init,
    jboolean is_spare_renderer) {
  ChildProcessTerminationInfo* info =
      reinterpret_cast<ChildProcessTerminationInfo*>(termination_info_ptr);
  info->binding_state =
      static_cast<base::android::ChildBindingState>(binding_state);
  info->was_killed_intentionally_by_browser = killed_by_us;
  info->threw_exception_during_init = exception_during_init;
  info->clean_exit = clean_exit;
  info->is_spare_renderer = is_spare_renderer;
}

static jboolean
JNI_ChildProcessLauncherHelperImpl_ServiceGroupImportanceEnabled(JNIEnv* env) {
  // Not this is called on the launcher thread, not UI thread.
  //
  // Note that service grouping is mandatory for site isolation on pre-U devices
  // to avoid cached process limit. By service grouping, cached chrome renderer
  // processes in a group are counted as one. On pre-U devices the cached
  // process limit is usually 32 or such. U+ devices has a larger limit 1024 or
  // such.
  return (SiteIsolationPolicy::AreIsolatedOriginsEnabled() ||
          SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
          SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled() ||
          SiteIsolationPolicy::ArePreloadedIsolatedOriginsEnabled()) &&
         (base::android::android_info::sdk_int() <
              base::android::android_info::SDK_VERSION_U ||
          base::FeatureList::IsEnabled(kServiceGroupImportance));
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(const base::Process& process,
                                                  int exit_code) {
  GetProcessLauncherTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&StopChildProcess, process.Handle()));
  return true;
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  VLOG(1) << "ChromeProcess: Stopping process with handle "
          << process.process.Handle();
  StopChildProcess(process.process.Handle());
}

base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  return base::File(base::android::OpenApkAsset(path.value(), region));
}

base::android::ChildBindingState
ChildProcessLauncherHelper::GetEffectiveChildBindingState() {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return static_cast<base::android::ChildBindingState>(
      Java_ChildProcessLauncherHelperImpl_getEffectiveChildBindingState(
          env, java_peer_));
}

void ChildProcessLauncherHelper::DumpProcessStack(
    const base::Process& process) {
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  return Java_ChildProcessLauncherHelperImpl_dumpProcessStack(env, java_peer_,
                                                              process.Handle());
}

void ChildProcessLauncherHelper::SetRenderProcessPriorityOnLauncherThread(
    base::Process process,
    const RenderProcessPriority& priority,
    base::TimeTicks post_from_ui_thread_time) {
  TRACE_EVENT(
      "content",
      "ChildProcessLauncherHelper::SetRenderProcessPriorityOnLauncherThread",
      "pid", process.Handle());
  JNIEnv* env = AttachCurrentThread();
  DCHECK(env);
  jint result = Java_ChildProcessLauncherHelperImpl_setPriority(
      env, java_peer_, process.Handle(), priority.visible,
      priority.has_media_stream, priority.has_immersive_xr_session,
      priority.has_foreground_service_worker, priority.frame_depth,
      priority.intersects_viewport, priority.boost_for_pending_views,
      priority.boost_for_loading, priority.is_spare_renderer,
      static_cast<jint>(priority.importance));
  if (result != static_cast<jint>(SpareRendererPriority::SPARE_NO_CHANGE)) {
    client_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ChildProcessLauncherHelper::
                           OnSpareRendererPriorityGraduatedOnClientThread,
                       this,
                       result == static_cast<jint>(
                                     SpareRendererPriority::SPARE_GRADUATED)));
  }
  base::UmaHistogramMicrosecondsTimes(
      "Android.ChildProcessBinding.SetPriorityLatency",
      base::TimeTicks::Now() - post_from_ui_thread_time);
}

// Called from ChildProcessLauncher.java when the ChildProcess was started.
// |handle| is the processID of the child process as originated in Java, 0 if
// the ChildProcess could not be created.
void ChildProcessLauncherHelper::OnChildProcessStarted(JNIEnv*, jint handle) {
  DCHECK(CurrentlyOnProcessLauncherTaskRunner());
  scoped_refptr<ChildProcessLauncherHelper> ref(this);
  Release();  // Balances with LaunchProcessOnLauncherThread.

  int launch_result = (handle == base::kNullProcessHandle)
                          ? LAUNCH_RESULT_FAILURE
                          : LAUNCH_RESULT_SUCCESS;

  ChildProcessLauncherHelper::Process process;
  process.process = base::Process(handle);
  PostLaunchOnLauncherThread(std::move(process), launch_result);
}

void ChildProcessLauncherHelper::OnSpareRendererPriorityGraduatedOnClientThread(
    bool is_alive) {
  if (child_process_launcher_) {
    child_process_launcher_->OnSpareRendererPriorityGraduated(is_alive);
  }
}

}  // namespace internal

}  // namespace content

DEFINE_JNI(ChildProcessLauncherHelperImpl)
