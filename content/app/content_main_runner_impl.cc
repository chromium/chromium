// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/app/content_main_runner_impl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/allocator/allocator_check.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory_hooks.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_source.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/power_monitor/make_power_monitor_device_source.h"
#include "components/variations/variations_ids_provider.h"
#include "content/app/mojo_ipc_support.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_process_io_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/browser/tracing/memory_instrumentation_util.h"
#include "content/browser/utility_process_host.h"
#include "content/child/field_trial.h"
#include "content/common/content_constants_internal.h"
#include "content/common/process_visibility_tracker.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/tracing_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "gin/thread_isolation.h"
#include "gin/v8_initializer.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/first_party_sets/local_set_declaration.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/features.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/tflite/buildflags.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_WIN)
#include <malloc.h>
#include <cstring>

#include "base/trace_event/trace_event_etw_export_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/display/win/dpi.h"
#elif BUILDFLAG(IS_MAC)
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_IOS)
#include "base/threading/thread_restrictions.h"
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#include "content/browser/posix_file_descriptor_info_impl.h"
#include "content/public/common/content_descriptors.h"

#if !BUILDFLAG(IS_MAC)
#include "content/public/common/zygote/zygote_fork_delegate_linux.h"
#endif

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_path_watcher_inotify.h"
#include "base/native_library.h"
#include "base/rand_util.h"
#include "content/public/common/zygote/sandbox_support_linux.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/webrtc_overrides/init_webrtc.h"  // nogncheck

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#include "chromeos/startup/startup_switches.h"
#endif

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/content_plugin_info.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#endif

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(USE_ZYGOTE)
#include "base/stack_canary_linux.h"
#include "content/browser/sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/shared_file_util.h"
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/zygote/sandbox_support_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#include "content/zygote/zygote_main.h"
#include "media/base/media_switches.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#include "content/browser/android/battery_metrics.h"
#include "content/browser/android/browser_startup_controller.h"
#include "content/common/android/cpu_time_metrics.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/system_info.h"
#endif

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/cpuinfo/src/include/cpuinfo.h"
#endif

#if defined(ADDRESS_SANITIZER)
#include "base/debug/asan_service.h"
#endif

namespace content {
extern int GpuMain(MainFunctionParams);
#if BUILDFLAG(ENABLE_PPAPI)
extern int PpapiPluginMain(MainFunctionParams);
#endif
extern int RendererMain(MainFunctionParams);
extern int UtilityMain(MainFunctionParams);
}  // namespace content

namespace content {

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA) && BUILDFLAG(IS_ANDROID)
#if defined __LP64__
#define kV8SnapshotDataDescriptor kV8Snapshot64DataDescriptor
#define kV8ContextSnapshotDataDescriptor kV8ContextSnapshot64DataDescriptor
#else
#define kV8SnapshotDataDescriptor kV8Snapshot32DataDescriptor
#define kV8ContextSnapshotDataDescriptor kV8ContextSnapshot32DataDescriptor
#endif
#endif

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)

gin::V8SnapshotFileType GetSnapshotType(const base::CommandLine& command_line) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
  if (command_line.HasSwitch(switches::kUseContextSnapshotSwitch)) {
    return gin::V8SnapshotFileType::kWithAdditionalContext;
  }
  return gin::V8SnapshotFileType::kDefault;
#elif BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  return gin::V8SnapshotFileType::kWithAdditionalContext;
#else
  return gin::V8SnapshotFileType::kDefault;
#endif
}

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
std::string GetSnapshotDataDescriptor(const base::CommandLine& command_line) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(INCLUDE_BOTH_V8_SNAPSHOTS)
  if (command_line.HasSwitch(switches::kUseContextSnapshotSwitch)) {
    return kV8ContextSnapshotDataDescriptor;
  }
  return kV8SnapshotDataDescriptor;
#elif BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  return kV8ContextSnapshotDataDescriptor;
#else
  return kV8SnapshotDataDescriptor;
#endif
}

#endif

#if defined(ADDRESS_SANITIZER)
NO_SANITIZE("address")
void AsanProcessInfoCB(const char*, bool*) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_WIN)
  std::string cmd_string = base::WideToUTF8(cmd_line->GetCommandLineString());
#else
  std::string cmd_string = cmd_line->GetCommandLineString();
#endif
  base::debug::AsanService::GetInstance()->Log("\nCommand line: `%s`\n",
                                               cmd_string.c_str());
}
#endif  // defined(ADDRESS_SANITIZER)

void LoadV8SnapshotFile(const base::CommandLine& command_line) {
  const gin::V8SnapshotFileType snapshot_type = GetSnapshotType(command_line);
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  base::FileDescriptorStore& file_descriptor_store =
      base::FileDescriptorStore::GetInstance();
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd = file_descriptor_store.MaybeTakeFD(
      GetSnapshotDataDescriptor(command_line), &region);
  if (fd.is_valid()) {
    base::File file(std::move(fd));
    gin::V8Initializer::LoadV8SnapshotFromFile(std::move(file), &region,
                                               snapshot_type);
    return;
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)

  gin::V8Initializer::LoadV8Snapshot(snapshot_type);
}

bool ShouldLoadV8Snapshot(const base::CommandLine& command_line,
                          const std::string& process_type) {
  // The gpu does not need v8, and the browser only needs v8 when in single
  // process mode.
  if (process_type == switches::kGpuProcess ||
      (process_type.empty() &&
       !command_line.HasSwitch(switches::kSingleProcess))) {
    return false;
  }
  return true;
}

#endif  // V8_USE_EXTERNAL_STARTUP_DATA

void LoadV8SnapshotIfNeeded(const base::CommandLine& command_line,
                            const std::string& process_type) {
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  if (ShouldLoadV8Snapshot(command_line, process_type))
    LoadV8SnapshotFile(command_line);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

#if BUILDFLAG(USE_ZYGOTE)
pid_t LaunchZygoteHelper(base::CommandLine* cmd_line,
                         base::ScopedFD* control_fd) {
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  static const char* const kForwardSwitches[] = {
      switches::kAllowCommandLinePlugins,
      switches::kClearKeyCdmPathForTesting,
      switches::kEnableLogging,  // Support, e.g., --enable-logging=stderr.
      // Need to tell the zygote that it is headless so that we don't try to use
      // the wrong type of main delegate.
      switches::kHeadless,
      // Zygote process needs to know what resources to have loaded when it
      // becomes a renderer process.
      switches::kForceDeviceScaleFactor,
      switches::kLoggingLevel,
      switches::kPpapiInProcess,
      switches::kRegisterPepperPlugins,
      switches::kV,
      switches::kVModule,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      switches::kEnableResourcesFileSharing,
      switches::kCrosWidevineBundledDir,
      switches::kCrosWidevineComponentUpdatedHintFile,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  };
  cmd_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                             kForwardSwitches);

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line, -1);

  // Start up the sandbox host process and get the file descriptor for the
  // sandboxed processes to talk to it.
  std::unique_ptr<PosixFileDescriptorInfo> additional_remapped_fds(
      PosixFileDescriptorInfoImpl::Create());
  additional_remapped_fds->Share(
      GetSandboxFD(), SandboxHostLinux::GetInstance()->GetChildSocket());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  GetContentClient()->browser()->GetAdditionalMappedFilesForZygote(
      cmd_line, additional_remapped_fds.get());
#endif

  return ZygoteHostImpl::GetInstance()->LaunchZygote(
      cmd_line, control_fd, additional_remapped_fds->GetMapping());
}

// Initializes the Zygote sandbox host. No thread should be created before this
// call, as InitializeZygoteSandboxForBrowserProcess() will end-up using fork().
void InitializeZygoteSandboxForBrowserProcess(
    const base::CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "SetupSandbox");
  // SandboxHostLinux needs to be initialized even if the sandbox and
  // zygote are both disabled. It initializes the sandboxed process socket.
  SandboxHostLinux::GetInstance()->Init();

  if (parsed_command_line.HasSwitch(switches::kNoZygote)) {
    if (!parsed_command_line.HasSwitch(sandbox::policy::switches::kNoSandbox)) {
      LOG(ERROR) << "Zygote cannot be disabled if sandbox is enabled."
                 << " Use --no-zygote together with --no-sandbox";
      exit(EXIT_FAILURE);
    }
    return;
  }

  // Tickle the zygote host so it forks now.
  ZygoteHostImpl::GetInstance()->Init(parsed_command_line);
  if (!parsed_command_line.HasSwitch(switches::kNoUnsandboxedZygote)) {
    CreateUnsandboxedZygote(base::BindOnce(LaunchZygoteHelper));
  }
  ZygoteCommunication* generic_zygote =
      CreateGenericZygote(base::BindOnce(LaunchZygoteHelper));

  // This operation is done through the ZygoteHostImpl as a proxy because of
  // race condition concerns.
  ZygoteHostImpl::GetInstance()->SetRendererSandboxStatus(
      generic_zygote->GetSandboxStatus());
}
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PPAPI)
// Loads the (native) libraries but does not initialize them (i.e., does not
// call PPP_InitializeModule). This is needed by the zygote on Linux to get
// access to the plugins before entering the sandbox.
void PreloadPepperPlugins() {
  std::vector<ContentPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (const auto& plugin : plugins) {
    if (!plugin.is_internal) {
      base::NativeLibraryLoadError error;
      base::NativeLibrary library =
          base::LoadNativeLibrary(plugin.path, &error);
      LOG_IF(ERROR, !library) << "Unable to load plugin " << plugin.path.value()
                              << " " << error.ToString();
    }
  }
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Loads registered library CDMs but does not initialize them. This is needed by
// the zygote on Linux to get access to the CDMs before entering the sandbox.
void PreloadLibraryCdms() {
  std::vector<CdmInfo> cdms;
  GetContentClient()->AddContentDecryptionModules(&cdms, nullptr);
  for (const auto& cdm : cdms) {
    base::NativeLibraryLoadError error;
    base::NativeLibrary library = base::LoadNativeLibrary(cdm.path, &error);
    LOG_IF(ERROR, !library) << "Unable to load CDM " << cdm.path.value()
                            << " (error: " << error.ToString() << ")";
  }
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void PreSandboxInit() {
  // Ensure the /dev/urandom is opened.
  base::GetUrandomFD();

  // May use sysinfo(), sched_getaffinity(), and open various /sys/ and /proc/
  // files.
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::NumberOfProcessors();
  base::SysInfo::NumberOfEfficientProcessors();

  // Pre-acquire resources needed by BoringSSL. See
  // https://boringssl.googlesource.com/boringssl/+/HEAD/SANDBOXING.md
  CRYPTO_pre_sandbox_init();

  // Pre-read /proc/sys/fs/inotify/max_user_watches so it doesn't have to be
  // allowed by the sandbox.
  base::GetMaxNumberOfInotifyWatches();

#if BUILDFLAG(ENABLE_PPAPI)
  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PreloadPepperPlugins();
#endif
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Ensure access to the library CDMs before the sandbox is turned on.
  PreloadLibraryCdms();
#endif
  InitializeWebRtcModuleBeforeSandbox();

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // cpuinfo needs to parse /proc/cpuinfo, or its equivalent.
  if (!cpuinfo_initialize()) {
    LOG(ERROR) << "Failed to initialize cpuinfo";
  }
#endif

  // Preload and cache the results since the methods may use the prlimit64
  // system call that is not allowed by all sandbox types.
  base::internal::CanUseBackgroundThreadTypeForWorkerThread();
  base::internal::CanUseUtilityThreadTypeForWorkerThread();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

mojo::ScopedMessagePipeHandle MaybeAcceptMojoInvitation() {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (!mojo::PlatformChannel::CommandLineHasPassedEndpoint(command_line))
    return {};

  mojo::PlatformChannelEndpoint endpoint =
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
  return invitation.ExtractMessagePipe(0);
}

#if BUILDFLAG(IS_WIN)
void HandleConsoleControlEventOnBrowserUiThread(DWORD control_type) {
  GetContentClient()->browser()->SessionEnding(control_type);
}

// A console control event handler for browser processes that initiates end
// session handling on the main thread and hangs the control thread.
BOOL WINAPI BrowserConsoleControlHandler(DWORD control_type) {
  BrowserTaskExecutor::GetUIThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&HandleConsoleControlEventOnBrowserUiThread,
                                control_type));

  // Block the control thread while waiting for SessionEnding to be handled.
  base::PlatformThread::Sleep(base::Hours(1));

  // This should never be hit. The process will be terminated either by
  // ContentBrowserClient::SessionEnding or by Windows, if the former takes too
  // long.
  return TRUE;  // Handled.
}

// A console control event handler for non-browser processes that hangs the
// control thread. The event will be handled by the browser process.
BOOL WINAPI OtherConsoleControlHandler(DWORD control_type) {
  // Block the control thread while waiting for the browser process.
  base::PlatformThread::Sleep(base::Hours(1));

  // This should never be hit. The process will be terminated by the browser
  // process or by Windows, if the former takes too long.
  return TRUE;  // Handled.
}

void InstallConsoleControlHandler(bool is_browser_process) {
  if (!::SetConsoleCtrlHandler(is_browser_process
                                   ? &BrowserConsoleControlHandler
                                   : &OtherConsoleControlHandler,
                               /*Add=*/TRUE)) {
    DPLOG(ERROR) << "Failed to set console hook function";
  }
}
#endif  // BUILDFLAG(IS_WIN)

bool ShouldAllowSystemTracingConsumer() {
// System tracing consumer support is currently only supported on ChromeOS.
// TODO(crbug.com/40167100): Also enable for Lacros-Chrome.
#if BUILDFLAG(IS_CHROMEOS)
  // The consumer should only be enabled when the delegate allows it.
  return GetContentClient()->browser()->IsSystemWideTracingEnabled();
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void CreateChildThreadPool(const std::string& process_type) {
  // Thread pool should only be initialized once.
  DCHECK(!base::ThreadPoolInstance::Get());
  std::string_view thread_pool_name;
  if (process_type == switches::kGpuProcess)
    thread_pool_name = "GPU";
  else if (process_type == switches::kRendererProcess)
    thread_pool_name = "Renderer";
  else
    thread_pool_name = "ContentChild";
  base::ThreadPoolInstance::Create(thread_pool_name);
}

}  // namespace

class ContentClientCreator {
 public:
  static void Create(ContentMainDelegate* delegate) {
    ContentClient* client = delegate->CreateContentClient();
    DCHECK(client);
    SetContentClient(client);
  }
};

class ContentClientInitializer {
 public:
  static void Set(const std::string& process_type,
                  ContentMainDelegate* delegate) {
    ContentClient* content_client = GetContentClient();
    if (process_type.empty())
      content_client->browser_ = delegate->CreateContentBrowserClient();

    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    if (process_type == switches::kGpuProcess ||
        cmd->HasSwitch(switches::kSingleProcess) ||
        (process_type.empty() && cmd->HasSwitch(switches::kInProcessGPU)))
      content_client->gpu_ = delegate->CreateContentGpuClient();

    if (process_type == switches::kRendererProcess ||
        cmd->HasSwitch(switches::kSingleProcess))
      content_client->renderer_ = delegate->CreateContentRendererClient();

    if (process_type == switches::kUtilityProcess ||
        cmd->HasSwitch(switches::kSingleProcess))
      content_client->utility_ = delegate->CreateContentUtilityClient();
  }
};

// We dispatch to a process-type-specific FooMain() based on a command-line
// flag.  This struct is used to build a table of (flag, main function) pairs.
struct MainFunction {
  const char* name;
  int (*function)(MainFunctionParams);
};

#if BUILDFLAG(USE_ZYGOTE)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR int RunZygote(ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    {switches::kGpuProcess, GpuMain},
    {switches::kRendererProcess, RendererMain},
    {switches::kUtilityProcess, UtilityMain},
#if BUILDFLAG(ENABLE_PPAPI)
    {switches::kPpapiPluginProcess, PpapiPluginMain},
#endif
  };

  std::vector<std::unique_ptr<ZygoteForkDelegate>> zygote_fork_delegates;
  delegate->ZygoteStarting(&zygote_fork_delegates);

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(std::move(zygote_fork_delegates))) {
    return 1;
  }

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  CreateChildThreadPool(process_type);

  // Re-randomize our stack canary, so processes don't share a single
  // stack canary.
  base::ScopedClosureRunner stack_canary_debug_message;
  if (command_line->GetSwitchValueASCII(switches::kChangeStackGuardOnFork) ==
      switches::kChangeStackGuardOnForkEnabled) {
    base::ResetStackCanaryIfPossible();
    stack_canary_debug_message.ReplaceClosure(
        base::BindOnce(&base::SetStackSmashingEmitsDebugMessage));
  }

  // The zygote sets up base::GlobalDescriptors with all of the FDs passed to
  // the new child, so populate base::FileDescriptorStore with a subset of the
  // FDs currently stored in base::GlobalDescriptors.
  PopulateFileDescriptorStoreFromGlobalDescriptors();

  delegate->ZygoteForked();

  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterZygoteFork(
      process_type);

  ContentClientInitializer::Set(process_type, delegate);

  const ContentMainDelegate::InvokedInChildProcess invoked_in_child{
      .is_zygote_child = true};
  if (delegate->ShouldCreateFeatureList(invoked_in_child)) {
    InitializeFieldTrialAndFeatureList();
  }
  if (delegate->ShouldInitializeMojo(invoked_in_child)) {
    InitializeMojoCore();
  }
  delegate->PostEarlyInitialization(invoked_in_child);

  base::allocator::PartitionAllocSupport::Get()
      ->ReconfigureAfterFeatureListInit(process_type);

  // Ensure media library is initialized after feature list initialization.
  media::InitializeMediaLibrary();

  MainFunctionParams main_params(command_line);
  main_params.zygote_child = true;
  main_params.needs_startup_tracing_after_mojo_init = true;

  // The hang watcher needs to be created once the feature list is available
  // but before the IO thread is started.
  base::ScopedClosureRunner unregister_thread_closure;
  if (base::HangWatcher::IsEnabled()) {
    base::HangWatcher::CreateHangWatcherInstance();
    unregister_thread_closure = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kMainThread);

    // If the process is unsandboxed the HangWatcher can start now. Otherwise,
    // the sandbox can't be initialized with multiple threads, so the
    // HangWatcher will be started after the sandbox is initialized.
    if (sandbox::policy::IsUnsandboxedSandboxType(
            sandbox::policy::SandboxTypeFromCommandLine(*command_line))) {
      base::HangWatcher::GetInstance()->Start();
    } else {
      main_params.hang_watcher_not_started_time = base::TimeTicks::Now();
    }
  }

  for (auto& kMainFunction : kMainFunctions) {
    if (process_type == kMainFunction.name) {
      return kMainFunction.function(std::move(main_params));
    }
  }

  auto exit_code = delegate->RunProcess(process_type, std::move(main_params));
  DCHECK(absl::holds_alternative<int>(exit_code));
  DCHECK_GE(absl::get<int>(exit_code), 0);
  return absl::get<int>(exit_code);
}
#endif  // BUILDFLAG(USE_ZYGOTE)

static void RegisterMainThreadFactories() {
  UtilityProcessHost::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
}

// Run the main function for browser process.
// Returns the exit code for this process.
int RunBrowserProcessMain(MainFunctionParams main_function_params,
                          ContentMainDelegate* delegate) {
#if BUILDFLAG(IS_WIN)
  if (delegate->ShouldHandleConsoleControlEvents())
    InstallConsoleControlHandler(/*is_browser_process=*/true);
#endif
  auto exit_code = delegate->RunProcess("", std::move(main_function_params));
  if (absl::holds_alternative<int>(exit_code)) {
    DCHECK_GE(absl::get<int>(exit_code), 0);
    return absl::get<int>(exit_code);
  }
  return BrowserMain(std::move(absl::get<MainFunctionParams>(exit_code)));
}

// Run the FooMain() for a given process type.
// Returns the exit code for this process.
// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR int RunOtherNamedProcessTypeMain(
    const std::string& process_type,
    MainFunctionParams main_function_params,
    ContentMainDelegate* delegate) {
#if BUILDFLAG(IS_MAC)
  base::Process::SetCurrentTaskDefaultRole();
#endif
#if BUILDFLAG(IS_WIN)
  if (delegate->ShouldHandleConsoleControlEvents())
    InstallConsoleControlHandler(/*is_browser_process=*/false);
#endif
  static const MainFunction kMainFunctions[] = {
#if BUILDFLAG(ENABLE_PPAPI)
    {switches::kPpapiPluginProcess, PpapiPluginMain},
#endif  // BUILDFLAG(ENABLE_PPAPI)
    {switches::kUtilityProcess, UtilityMain},
    {switches::kRendererProcess, RendererMain},
    {switches::kGpuProcess, GpuMain},
  };

  // The hang watcher needs to be started once the feature list is available
  // but before the IO thread is started.
  base::ScopedClosureRunner unregister_thread_closure;
  if (base::HangWatcher::IsEnabled()) {
    base::HangWatcher::CreateHangWatcherInstance();
    unregister_thread_closure = base::HangWatcher::RegisterThread(
        base::HangWatcher::ThreadType::kMainThread);
    bool start_hang_watcher_now;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // On Linux/ChromeOS, the HangWatcher can't start until after the sandbox is
    // initialized, because the sandbox can't be started with multiple threads.
    // TODO(mpdenton): start the HangWatcher after the sandbox is initialized.
    // Currently there are no sandboxed processes that aren't launched from the
    // zygote so this doesn't disable the HangWatcher anywhere.
    start_hang_watcher_now = sandbox::policy::IsUnsandboxedSandboxType(
        sandbox::policy::SandboxTypeFromCommandLine(
            *main_function_params.command_line));
#else
    start_hang_watcher_now = true;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    if (start_hang_watcher_now) {
      base::HangWatcher::GetInstance()->Start();
    } else {
      main_function_params.hang_watcher_not_started_time =
          base::TimeTicks::Now();
    }
  }

  for (size_t i = 0; i < std::size(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name) {
      auto exit_code =
          delegate->RunProcess(process_type, std::move(main_function_params));
      if (absl::holds_alternative<int>(exit_code)) {
        DCHECK_GE(absl::get<int>(exit_code), 0);
        return absl::get<int>(exit_code);
      }
      return kMainFunctions[i].function(
          std::move(absl::get<MainFunctionParams>(exit_code)));
    }
  }

#if BUILDFLAG(USE_ZYGOTE)
  // Zygote startup is special -- see RunZygote comments above
  // for why we don't use ZygoteMain directly.
  if (process_type == switches::kZygoteProcess)
    return RunZygote(delegate);
#endif  // BUILDFLAG(USE_ZYGOTE)

  // If it's a process we don't know about, the embedder should know.
  auto exit_code =
      delegate->RunProcess(process_type, std::move(main_function_params));
  DCHECK(absl::holds_alternative<int>(exit_code));
  DCHECK_GE(absl::get<int>(exit_code), 0);
  return absl::get<int>(exit_code);
}

// static
std::unique_ptr<ContentMainRunnerImpl> ContentMainRunnerImpl::Create() {
  return std::make_unique<ContentMainRunnerImpl>();
}

ContentMainRunnerImpl::ContentMainRunnerImpl() = default;

ContentMainRunnerImpl::~ContentMainRunnerImpl() {
  if (is_initialized_ && !is_shutdown_)
    Shutdown();
}

int ContentMainRunnerImpl::TerminateForFatalInitializationError() {
  return delegate_->TerminateForFatalInitializationError();
}

int ContentMainRunnerImpl::Initialize(ContentMainParams params) {
  // ContentMainDelegate is used by this class, not forwarded to embedders.
  delegate_ = std::exchange(params.delegate, nullptr);
  content_main_params_.emplace(std::move(params));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

  // Create and start the ThreadPool early to allow the rest of the startup
  // code to use the thread_pool.h API.
  if (!process_type.empty()) {
    if (process_type != switches::kZygoteProcess) {
      // Zygotes will run this at a later point in time when the command line
      // has been updated.
      CreateChildThreadPool(process_type);
    }
  } else {
    // Create and start the ThreadPool early to allow the rest of the startup
    // code to use the thread_pool.h API.
    delegate_->CreateThreadPool("Browser");
    DCHECK_NE(base::ThreadPoolInstance::Get(), nullptr);
  }

#if !BUILDFLAG(IS_WIN)

  [[maybe_unused]] base::GlobalDescriptors* g_fds =
      base::GlobalDescriptors::GetInstance();

// On Android, the shared descriptors are passed through the Java service,
// which takes care of updating these mappings; otherwise, we need to update
// the mappings explicitly.
#if !BUILDFLAG(IS_ANDROID)
  g_fds->Set(kMojoIPCChannel,
             kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);
  g_fds->Set(kFieldTrialDescriptor,
             kFieldTrialDescriptor + base::GlobalDescriptors::kBaseDescriptor);
  g_fds->Set(kHistogramSharedMemoryDescriptor,
             kHistogramSharedMemoryDescriptor +
                 base::GlobalDescriptors::kBaseDescriptor);
  g_fds->Set(kTraceConfigSharedMemoryDescriptor,
             kTraceConfigSharedMemoryDescriptor +
                 base::GlobalDescriptors::kBaseDescriptor);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_OPENBSD)
  g_fds->Set(kCrashDumpSignal,
             kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_OPENBSD)

#endif  // !BUILDFLAG(IS_WIN)

  is_initialized_ = true;

  // Enable startup tracing asap now that mojo's core is initialized, to avoid
  // early TRACE_EVENT calls being ignored.
  //
  // Startup tracing flags are not (and should not be) passed to Zygote
  // processes. We will enable tracing when forked, if needed.
  bool enable_startup_tracing = process_type != switches::kZygoteProcess;
#if BUILDFLAG(USE_ZYGOTE)
  // In the browser process, we have to enable startup tracing after
  // InitializeZygoteSandboxForBrowserProcess() is run below, because that
  // function forks and may call trace macros in the forked process.
  if (process_type.empty()) {
    enable_startup_tracing = false;
  }
#endif  // BUILDFLAG(USE_ZYGOTE)
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  // A sandboxed process won't be able to allocate the SMB needed for startup
  // tracing until Mojo IPC support is brought up, at which point the Mojo
  // broker will transparently broker the SMB creation.
  if (!sandbox::policy::IsUnsandboxedSandboxType(
          sandbox::policy::SandboxTypeFromCommandLine(command_line))) {
    enable_startup_tracing = false;
    needs_startup_tracing_after_mojo_init_ = true;
  }
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  if (enable_startup_tracing) {
    tracing::EnableStartupTracingIfNeeded();
  }
  TRACE_EVENT0("startup,benchmark,rail", "ContentMainRunnerImpl::Initialize");

// The exit manager is in charge of calling the dtors of singleton objects.
// On Android, AtExitManager is set up when library is loaded.
// A consequence of this is that you can't use the ctor/dtor-based
// TRACE_EVENT methods on Linux or iOS builds till after we set this up.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (!content_main_params_->ui_task) {
    // When running browser tests, don't create a second AtExitManager as that
    // interfers with shutdown when objects created before ContentMain is
    // called are destructed when it returns.
    exit_manager_ = std::make_unique<base::AtExitManager>();
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA)
  // Cache the system info for this process.
  // This avoids requiring that all callers of certain base:: functions first
  // ensure the cache is populated.
  // Making the blocking call now also avoids the potential for blocking later
  // in when it might be user-visible.
  if (!base::FetchAndCacheSystemInfo()) {
    return TerminateForFatalInitializationError();
  }
#endif

  if (!GetContentClient())
    ContentClientCreator::Create(delegate_);
  std::optional<int> basic_startup_exit_code =
      delegate_->BasicStartupComplete();
  if (basic_startup_exit_code.has_value())
    return basic_startup_exit_code.value();

  base::allocator::PartitionAllocSupport::Get()->ReconfigureEarlyish(
      process_type);

#if BUILDFLAG(IS_WIN)
  if (command_line.HasSwitch(switches::kDeviceScaleFactor)) {
    std::string scale_factor_string =
        command_line.GetSwitchValueASCII(switches::kDeviceScaleFactor);
    double scale_factor = 0;
    if (base::StringToDouble(scale_factor_string, &scale_factor))
      display::win::SetDefaultDeviceScaleFactor(scale_factor);
  }
#endif

  RegisterContentSchemes(delegate_->ShouldLockSchemeRegistry());
  ContentClientInitializer::Set(process_type, delegate_);

  // If we are on a platform where the default allocator is overridden (e.g.
  // with PartitionAlloc on most platforms) smoke-tests that the overriding
  // logic is working correctly. If not causes a hard crash, as its unexpected
  // absence has security implications.
  CHECK(base::allocator::IsAllocatorInitialized());

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (!process_type.empty()) {
    // When you hit Ctrl-C in a terminal running the browser
    // process, a SIGINT is delivered to the entire process group.
    // When debugging the browser process via gdb, gdb catches the
    // SIGINT for the browser process (and dumps you back to the gdb
    // console) but doesn't for the child processes, killing them.
    // The fix is to have child processes ignore SIGINT; they'll die
    // on their own when the browser process goes away.
    //
    // Note that we *can't* rely on BeingDebugged to catch this case because
    // we are the child process, which is not being debugged.
    // TODO(evanm): move this to some shared subprocess-init function.
    if (!base::debug::BeingDebugged())
      signal(SIGINT, SIG_IGN);
  }
#endif

  RegisterPathProvider();

// On Android, InitializeICU() is called from content_jni_onload.cc
// so that it is available before Content::main() is called.
// https://crbug.com/1418738
#if !BUILDFLAG(IS_ANDROID)
  if (!base::i18n::InitializeICU())
    return TerminateForFatalInitializationError();
#endif  // BUILDFLAG(IS_ANDROID) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

  LoadV8SnapshotIfNeeded(command_line, process_type);

  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(
      base::BindRepeating([]() -> blink::OriginTrialPolicy* {
        if (auto* client = GetContentClient())
          return client->GetOriginTrialPolicy();
        return nullptr;
      }));

#if !defined(OFFICIAL_BUILD)
#if BUILDFLAG(IS_WIN)
  bool should_enable_stack_dump = !process_type.empty();
#else
  bool should_enable_stack_dump = true;
#endif
  // Print stack traces to stderr when crashes occur. This opens up security
  // holes so it should never be enabled for official builds. This needs to
  // happen before crash reporting is initialized (which for chrome happens in
  // the call to PreSandboxStartup() on the delegate below), because otherwise
  // this would interfere with signal handlers used by crash reporting.
  if (should_enable_stack_dump &&
      !command_line.HasSwitch(switches::kDisableInProcessStackTraces)) {
    base::debug::EnableInProcessStackDumping();
  }

  base::debug::VerifyDebugger();
#endif  // !defined(OFFICIAL_BUILD)

  delegate_->PreSandboxStartup();

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // instantiate the ThreadIsolatedAllocator before we spawn threads
  if (process_type == switches::kRendererProcess ||
      process_type == switches::kZygoteProcess) {
    gin::GetThreadIsolationData().InitializeBeforeThreadCreation();
  }
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#if BUILDFLAG(IS_POSIX)
  base::CheckPThreadStackMinIsSafe();
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_WIN)
  if (!sandbox::policy::Sandbox::Initialize(
          sandbox::policy::SandboxTypeFromCommandLine(command_line),
          content_main_params_->sandbox_info))
    return TerminateForFatalInitializationError();
#elif BUILDFLAG(IS_MAC)
  if (!sandbox::policy::IsUnsandboxedSandboxType(
          sandbox::policy::SandboxTypeFromCommandLine(command_line))) {
    // Verify that the sandbox was initialized prior to ContentMain using the
    // SeatbeltExecServer.
    CHECK(sandbox::Seatbelt::IsSandboxed());
  }
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // In sandboxed processes and zygotes, certain resource should be pre-warmed
  // as they cannot be initialized under a sandbox. In addition, loading these
  // resources in zygotes (including the unsandboxed zygote) allows them to be
  // initialized just once in the zygote, rather than in every forked child
  // process.
  if (!sandbox::policy::IsUnsandboxedSandboxType(
          sandbox::policy::SandboxTypeFromCommandLine(command_line)) ||
      process_type == switches::kZygoteProcess) {
    PreSandboxInit();
  }
#endif

  delegate_->SandboxInitialized(process_type);

#if BUILDFLAG(USE_ZYGOTE)
  if (process_type.empty()) {
    // The sandbox host needs to be initialized before forking a thread to
    // start IPC support, and after setting up the sandbox and invoking
    // SandboxInitialized().
    InitializeZygoteSandboxForBrowserProcess(
        *base::CommandLine::ForCurrentProcess());

    // We can only enable startup tracing after
    // InitializeZygoteSandboxForBrowserProcess(), because the latter may fork
    // and run code that calls trace event macros in the forked process (which
    // could cause all sorts of issues, like writing to the same tracing SMB
    // from two processes).
    tracing::EnableStartupTracingIfNeeded();
  }
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (process_type.empty()) {
    // Check if Landlock is supported.
    sandbox::policy::SandboxLinux::ReportLandlockStatus();
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // Return -1 to indicate no early termination.
  return -1;
}

void ContentMainRunnerImpl::ReInitializeParams(ContentMainParams new_params) {
  DCHECK(!content_main_params_);
  // Initialize() already set |delegate_|, expect the same one.
  DCHECK_EQ(delegate_, new_params.delegate);
  new_params.delegate = nullptr;
  content_main_params_.emplace(std::move(new_params));
}

// This function must be marked with NO_STACK_PROTECTOR or it may crash on
// return, see the --change-stack-guard-on-fork command line flag.
NO_STACK_PROTECTOR int ContentMainRunnerImpl::Run() {
  DCHECK(is_initialized_);
  DCHECK(content_main_params_);
  DCHECK(!is_shutdown_);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

#if defined(ADDRESS_SANITIZER)
  base::debug::AsanService::GetInstance()->Initialize();
  // Report the command line of this process in ASAN's Additional Info area.
  base::debug::AsanService::GetInstance()->AddErrorCallback(AsanProcessInfoCB);
#endif

  // Run this logic on all child processes.
  if (!process_type.empty()) {
    if (process_type != switches::kZygoteProcess) {
      if (delegate_->ShouldCreateFeatureList(
              ContentMainDelegate::InvokedInChildProcess())) {
        InitializeFieldTrialAndFeatureList();
      }
      if (delegate_->ShouldInitializeMojo(
              ContentMainDelegate::InvokedInChildProcess())) {
        InitializeMojoCore();
      }
      delegate_->PostEarlyInitialization(
          ContentMainDelegate::InvokedInChildProcess());

      base::allocator::PartitionAllocSupport::Get()
          ->ReconfigureAfterFeatureListInit(process_type);
    }
  }

  MainFunctionParams main_params(command_line);
  main_params.ui_task = std::move(content_main_params_->ui_task);
  main_params.created_main_parts_closure =
      std::move(content_main_params_->created_main_parts_closure);
  main_params.needs_startup_tracing_after_mojo_init =
      needs_startup_tracing_after_mojo_init_;
#if BUILDFLAG(IS_WIN)
  main_params.sandbox_info = content_main_params_->sandbox_info;
#elif BUILDFLAG(IS_MAC)
  main_params.autorelease_pool = content_main_params_->autorelease_pool;
#endif

  const bool start_minimal_browser = content_main_params_->minimal_browser_mode;

  // ContentMainParams cannot be wholesaled moved into MainFunctionParams
  // because MainFunctionParams is in common/ and can't depend on
  // ContentMainParams, but this is the effective intent.
  // |content_main_params_| shouldn't be reused after being handed off to
  // RunBrowser/RunOtherNamedProcessTypeMain below.
  content_main_params_.reset();

  RegisterMainThreadFactories();

  if (process_type.empty())
    return RunBrowser(std::move(main_params), start_minimal_browser);

  return RunOtherNamedProcessTypeMain(process_type, std::move(main_params),
                                      delegate_);
}

int ContentMainRunnerImpl::RunBrowser(MainFunctionParams main_params,
                                      bool start_minimal_browser) {
  TRACE_EVENT_INSTANT0("startup", "ContentMainRunnerImpl::RunBrowser(begin)",
                       TRACE_EVENT_SCOPE_THREAD);
  if (is_browser_main_loop_started_)
    return -1;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    mojo::SyncCallRestrictions::DisableSyncCallInterrupts();
  }

  if (!mojo_ipc_support_) {
    const ContentMainDelegate::InvokedInBrowserProcess invoked_in_browser{
        .is_running_test = !main_params.ui_task.is_null()};
    if (delegate_->ShouldCreateFeatureList(invoked_in_browser)) {
      // This is intentionally leaked since it needs to live for the duration
      // of the process and there's no benefit in cleaning it up at exit.
      base::FieldTrialList* leaked_field_trial_list =
          SetUpFieldTrialsAndFeatureList().release();
      ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
      std::ignore = leaked_field_trial_list;
    }

    if (delegate_->ShouldInitializeMojo(invoked_in_browser)) {
      InitializeMojoCore();
    }

    std::optional<int> pre_browser_main_exit_code = delegate_->PreBrowserMain();
    if (pre_browser_main_exit_code.has_value())
      return pre_browser_main_exit_code.value();

#if BUILDFLAG(IS_WIN)
    if (l10n_util::GetLocaleOverrides().empty()) {
      // Override the configured locale with the user's preferred UI language.
      // Don't do this if the locale is already set, which is done by
      // integration tests to ensure tests always run with the same locale.
      l10n_util::OverrideLocaleWithUILanguageList();
    }
#endif

    // Register the TaskExecutor for posting task to the BrowserThreads. It is
    // incorrect to post to a BrowserThread before this point. This instantiates
    // and binds the MessageLoopForUI on the main thread (but it's only labeled
    // as BrowserThread::UI in BrowserMainLoop::CreateMainMessageLoop).
    BrowserTaskExecutor::Create();

    auto* provider = delegate_->CreateVariationsIdsProvider();
    if (!provider) {
      variations::VariationsIdsProvider::Create(
          variations::VariationsIdsProvider::Mode::kUseSignedInState);
    }

    std::optional<int> post_early_initialization_exit_code =
        delegate_->PostEarlyInitialization(invoked_in_browser);
    if (post_early_initialization_exit_code.has_value())
      return post_early_initialization_exit_code.value();

    // The hang watcher needs to be started once the feature list is available
    // but before the IO thread is started.
    if (base::HangWatcher::IsEnabled()) {
      base::HangWatcher::CreateHangWatcherInstance();

      // Register the main thread to the HangWatcher and never unregister it. It
      // is safe to keep this scope up to the end of the process since the
      // HangWatcher is a leaky instance.
      base::ScopedClosureRunner unregister_thread_closure(
          base::HangWatcher::RegisterThread(
              base::HangWatcher::ThreadType::kMainThread));
      std::ignore = unregister_thread_closure.Release();

      base::HangWatcher::GetInstance()->Start();
    }

    // The FeatureList needs to be created before starting the ThreadPool.
    StartBrowserThreadPool();

    BrowserTaskExecutor::PostFeatureListSetup();

    tracing::PerfettoTracedProcess::Get()
        ->SetAllowSystemTracingConsumerCallback(
            base::BindRepeating(&ShouldAllowSystemTracingConsumer));
    tracing::InitTracingPostThreadPoolStartAndFeatureList(
        /* enable_consumer */ true);

    // PowerMonitor is needed in reduced mode. BrowserMainLoop will safely skip
    // initializing it again if it has already been initialized.
    base::PowerMonitor::GetInstance()->Initialize(
        MakePowerMonitorDeviceSource());

    // Ensure the visibility tracker is created on the main thread.
    ProcessVisibilityTracker::GetInstance();

#if BUILDFLAG(IS_ANDROID)
    SetupCpuTimeMetrics();

    // Requires base::PowerMonitor to be initialized first.
    AndroidBatteryMetrics::CreateInstance();
#endif

    GetContentClient()->browser()->SetIsMinimalMode(start_minimal_browser);
    if (start_minimal_browser) {
      ForceInProcessNetworkService();
      // Minimal browser mode doesn't initialize First-Party Sets the "usual"
      // way, so we do it manually.
      content::FirstPartySetsHandlerImpl::GetInstance()->Init(
          base::FilePath(), net::LocalSetDeclaration());
    }

    discardable_shared_memory_manager_ =
        std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();

    mojo_ipc_support_ =
        std::make_unique<MojoIpcSupport>(BrowserTaskExecutor::CreateIOThread());

    GetContentClient()->browser()->BindBrowserControlInterface(
        MaybeAcceptMojoInvitation());

    download::SetIOTaskRunner(mojo_ipc_support_->io_thread()->task_runner());

    InitializeBrowserMemoryInstrumentationClient();

#if BUILDFLAG(IS_ANDROID)
    if (start_minimal_browser) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&MinimalBrowserStartupComplete));
    }
#endif
  }

  // No specified process type means this is the Browser process.
  base::allocator::PartitionAllocSupport::Get()
      ->ReconfigureAfterFeatureListInit("");
  base::allocator::PartitionAllocSupport::Get()->ReconfigureAfterTaskRunnerInit(
      "");

  if (start_minimal_browser) {
    DVLOG(0) << "Chrome is running in minimal browser mode.";
    return -1;
  }

  is_browser_main_loop_started_ = true;
  main_params.startup_data = mojo_ipc_support_->CreateBrowserStartupData();
  return RunBrowserProcessMain(std::move(main_params), delegate_);
}

void ContentMainRunnerImpl::Shutdown() {
  DCHECK(is_initialized_);
  DCHECK(!is_shutdown_);

#if BUILDFLAG(IS_IOS)
  // This would normally be handled by BrowserMainLoop shutdown, but since iOS
  // (like Android) does not run this shutdown, we also need to ensure that we
  // permit sync primitives during shutdown. If we don't do this, eg, tearing
  // down test fixtures will often fail.
  // TODO(crbug.com/40557572): ideally these would both be scoped allowances.
  // That would be one of the first step to ensure no persistent work is being
  // done after ThreadPoolInstance::Shutdown() in order to move towards atomic
  // shutdown.
  base::PermanentThreadAllowance::AllowBaseSyncPrimitives();
  base::PermanentThreadAllowance::AllowBlocking();
#endif

  mojo_ipc_support_.reset();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  delegate_->ProcessExiting(process_type);

  // The BrowserTaskExecutor needs to be destroyed before |exit_manager_|.
  BrowserTaskExecutor::Shutdown();

#if BUILDFLAG(IS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC
#endif  // BUILDFLAG(IS_WIN)

  exit_manager_.reset(nullptr);

  delegate_ = nullptr;
  is_shutdown_ = true;
}

// static
std::unique_ptr<ContentMainRunner> ContentMainRunner::Create() {
  return ContentMainRunnerImpl::Create();
}

}  // namespace content
