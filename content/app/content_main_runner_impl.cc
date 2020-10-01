// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_main_runner_impl.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/allocator_check.h"
#include "base/allocator/allocator_extension.h"
#include "base/allocator/buildflags.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_base.h"
#include "base/path_service.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/hang_watcher.h"
#include "base/trace_event/trace_event.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/app/mojo/mojo_init.h"
#include "content/app/service_manager_environment.h"
#include "content/browser/browser_main.h"
#include "content/browser/browser_process_sub_thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/gpu_main_thread_factory.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/browser/tracing/memory_instrumentation_util.h"
#include "content/browser/utility_process_host.h"
#include "content/child/field_trial.h"
#include "content/common/android/cpu_time_metrics.h"
#include "content/common/content_constants_internal.h"
#include "content/common/mojo_core_library_support.h"
#include "content/common/url_schemes.h"
#include "content/gpu/in_process_gpu_thread.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_descriptor_keys.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/zygote/zygote_buildflags.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/renderer/in_process_renderer_thread.h"
#include "content/utility/in_process_utility_thread.h"
#include "gin/v8_initializer.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/dynamic_library_support.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/features.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"

#if defined(OS_WIN)
#include <malloc.h>
#include <cstring>

#include "base/trace_event/trace_event_etw_export_win.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/display/win/dpi.h"
#elif defined(OS_MAC)
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif  // OS_WIN

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <signal.h>

#include "base/file_descriptor_store.h"
#include "base/posix/global_descriptors.h"
#include "content/public/common/content_descriptors.h"

#if !defined(OS_MAC)
#include "content/public/common/zygote/zygote_fork_delegate_linux.h"
#endif
#if !defined(OS_MAC) && !defined(OS_ANDROID)
#include "content/zygote/zygote_main.h"
#include "sandbox/linux/services/libc_interceptor.h"
#endif

#endif  // OS_POSIX || OS_FUCHSIA

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "base/native_library.h"
#include "base/rand_util.h"
#include "content/public/common/zygote/sandbox_support_linux.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_android.h"
#include "third_party/webrtc_overrides/init_webrtc.h"  // nogncheck

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_plugin_list.h"
#include "content/public/common/pepper_plugin_info.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"
#endif

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
#include "content/browser/sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/zygote/sandbox_support_linux.h"
#include "content/public/common/zygote/zygote_handle.h"
#include "media/base/media_switches.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/android/browser_startup_controller.h"
#include "content/common/android/cpu_affinity.h"
#endif

namespace content {
extern int GpuMain(const content::MainFunctionParams&);
#if BUILDFLAG(ENABLE_PLUGINS)
extern int PpapiPluginMain(const MainFunctionParams&);
extern int PpapiBrokerMain(const MainFunctionParams&);
#endif
extern int RendererMain(const content::MainFunctionParams&);
extern int UtilityMain(const MainFunctionParams&);
}  // namespace content

namespace content {

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA) && defined(OS_ANDROID)
#if defined __LP64__
#define kV8SnapshotDataDescriptor kV8Snapshot64DataDescriptor
#else
#define kV8SnapshotDataDescriptor kV8Snapshot32DataDescriptor
#endif
#endif

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
void LoadV8SnapshotFile() {
#if defined(USE_V8_CONTEXT_SNAPSHOT)
  static constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
      gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
  static const char* snapshot_data_descriptor =
      kV8ContextSnapshotDataDescriptor;
#else
  static constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
      gin::V8Initializer::V8SnapshotFileType::kDefault;
  static const char* snapshot_data_descriptor = kV8SnapshotDataDescriptor;
#endif  // USE_V8_CONTEXT_SNAPSHOT
  ALLOW_UNUSED_LOCAL(kSnapshotType);
  ALLOW_UNUSED_LOCAL(snapshot_data_descriptor);

#if defined(OS_POSIX) && !defined(OS_MAC)
  base::FileDescriptorStore& file_descriptor_store =
      base::FileDescriptorStore::GetInstance();
  base::MemoryMappedFile::Region region;
  base::ScopedFD fd =
      file_descriptor_store.MaybeTakeFD(snapshot_data_descriptor, &region);
  if (fd.is_valid()) {
    base::File file(std::move(fd));
    gin::V8Initializer::LoadV8SnapshotFromFile(std::move(file), &region,
                                               kSnapshotType);
    return;
  }
#endif  // OS_POSIX && !OS_MAC

  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
}
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

void InitializeV8IfNeeded(const base::CommandLine& command_line,
                          const std::string& process_type) {
  if (process_type == switches::kGpuProcess)
    return;

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  LoadV8SnapshotFile();
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
}

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
pid_t LaunchZygoteHelper(base::CommandLine* cmd_line,
                         base::ScopedFD* control_fd) {
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  static const char* const kForwardSwitches[] = {
      switches::kAndroidFontsPath,
      switches::kClearKeyCdmPathForTesting,
      switches::kEnableLogging,  // Support, e.g., --enable-logging=stderr.
      // Need to tell the zygote that it is headless so that we don't try to use
      // the wrong type of main delegate.
      switches::kHeadless,
      // Zygote process needs to know what resources to have loaded when it
      // becomes a renderer process.
      switches::kForceDeviceScaleFactor,
      switches::kLoggingLevel,
      switches::kMojoCoreLibraryPath,
      switches::kPpapiInProcess,
      switches::kRegisterPepperPlugins,
      switches::kV,
      switches::kVModule,
  };
  cmd_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                             kForwardSwitches, base::size(kForwardSwitches));

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(cmd_line, -1);

  // Start up the sandbox host process and get the file descriptor for the
  // sandboxed processes to talk to it.
  base::FileHandleMappingVector additional_remapped_fds;
  additional_remapped_fds.emplace_back(
      SandboxHostLinux::GetInstance()->GetChildSocket(), GetSandboxFD());

  return ZygoteHostImpl::GetInstance()->LaunchZygote(
      cmd_line, control_fd, std::move(additional_remapped_fds));
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
      LOG(ERROR) << "--no-sandbox should be used together with --no--zygote";
      exit(EXIT_FAILURE);
    }
    return;
  }

  // Tickle the zygote host so it forks now.
  ZygoteHostImpl::GetInstance()->Init(parsed_command_line);
  CreateUnsandboxedZygote(base::BindOnce(LaunchZygoteHelper));
  ZygoteHandle generic_zygote =
      CreateGenericZygote(base::BindOnce(LaunchZygoteHelper));

  // TODO(kerrnel): Investigate doing this without the ZygoteHostImpl as a
  // proxy. It is currently done this way due to concerns about race
  // conditions.
  ZygoteHostImpl::GetInstance()->SetRendererSandboxStatus(
      generic_zygote->GetSandboxStatus());
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_LINUX) || defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
// Loads the (native) libraries but does not initialize them (i.e., does not
// call PPP_InitializeModule). This is needed by the zygote on Linux to get
// access to the plugins before entering the sandbox.
void PreloadPepperPlugins() {
  std::vector<PepperPluginInfo> plugins;
  ComputePepperPluginList(&plugins);
  for (const auto& plugin : plugins) {
    if (!plugin.is_internal) {
      base::NativeLibraryLoadError error;
      base::NativeLibrary library =
          base::LoadNativeLibrary(plugin.path, &error);
      VLOG_IF(1, !library) << "Unable to load plugin " << plugin.path.value()
                           << " " << error.ToString();

      ignore_result(library);  // Prevent release-mode warning.
    }
  }
}
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Loads registered library CDMs but does not initialize them. This is needed by
// the zygote on Linux to get access to the CDMs before entering the sandbox.
void PreloadLibraryCdms() {
  std::vector<CdmInfo> cdms;
  GetContentClient()->AddContentDecryptionModules(&cdms, nullptr);
  for (const auto& cdm : cdms) {
    base::NativeLibraryLoadError error;
    base::NativeLibrary library = base::LoadNativeLibrary(cdm.path, &error);
    VLOG_IF(1, !library) << "Unable to load CDM " << cdm.path.value()
                         << " (error: " << error.ToString() << ")";
    ignore_result(library);  // Prevent release-mode warning.
  }
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
void PreSandboxInit() {
  // Pre-acquire resources needed by BoringSSL. See
  // https://boringssl.googlesource.com/boringssl/+/HEAD/SANDBOXING.md
  CRYPTO_pre_sandbox_init();

#if BUILDFLAG(ENABLE_PLUGINS)
  // Ensure access to the Pepper plugins before the sandbox is turned on.
  PreloadPepperPlugins();
#endif
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  // Ensure access to the library CDMs before the sandbox is turned on.
  PreloadLibraryCdms();
#endif
  InitializeWebRtcModule();

  // Set the android SkFontMgr for blink. We need to ensure this is done
  // before the sandbox is initialized to allow the font manager to access
  // font configuration files on disk.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAndroidFontsPath)) {
    std::string android_fonts_dir =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAndroidFontsPath);

    if (android_fonts_dir.size() > 0 && android_fonts_dir.back() != '/')
      android_fonts_dir += '/';

    SkFontMgr_Android_CustomFonts custom;
    custom.fSystemFontUse =
        SkFontMgr_Android_CustomFonts::SystemFontUse::kOnlyCustom;
    custom.fBasePath = android_fonts_dir.c_str();

    std::string font_config;
    std::string fallback_font_config;
    if (android_fonts_dir.find("kitkat") != std::string::npos) {
      font_config = android_fonts_dir + "system_fonts.xml";
      fallback_font_config = android_fonts_dir + "fallback_fonts.xml";
      custom.fFallbackFontsXml = fallback_font_config.c_str();
    } else {
      font_config = android_fonts_dir + "fonts.xml";
      custom.fFallbackFontsXml = nullptr;
    }
    custom.fFontsXml = font_config.c_str();
    custom.fIsolated = true;

    blink::WebFontRenderStyle::SetSkiaFontManager(
        SkFontMgr_New_Android(&custom));
  }
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

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
  int (*function)(const MainFunctionParams&);
};

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
// On platforms that use the zygote, we have a special subset of
// subprocesses that are launched via the zygote.  This function
// fills in some process-launching bits around ZygoteMain().
// Returns the exit code of the subprocess.
int RunZygote(ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
    {switches::kGpuProcess, GpuMain},
    {switches::kRendererProcess, RendererMain},
    {switches::kUtilityProcess, UtilityMain},
#if BUILDFLAG(ENABLE_PLUGINS)
    {switches::kPpapiPluginProcess, PpapiPluginMain},
#endif
  };

  std::vector<std::unique_ptr<ZygoteForkDelegate>> zygote_fork_delegates;
  delegate->ZygoteStarting(&zygote_fork_delegates);
  media::InitializeMediaLibrary();

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  PreSandboxInit();
#endif

  // This function call can return multiple times, once per fork().
  if (!ZygoteMain(std::move(zygote_fork_delegates))) {
    return 1;
  }

  delegate->ZygoteForked();

  // Zygote::HandleForkRequest may have reallocated the command
  // line so update it here with the new version.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  ContentClientInitializer::Set(process_type, delegate);

  MainFunctionParams main_params(command_line);
  main_params.zygote_child = true;

  InitializeFieldTrialAndFeatureList();
  delegate->PostFieldTrialInitialization();

  for (size_t i = 0; i < base::size(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name)
      return kMainFunctions[i].function(main_params);
  }

  return delegate->RunProcess(process_type, main_params);
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

static void RegisterMainThreadFactories() {
  UtilityProcessHost::RegisterUtilityMainThreadFactory(
      CreateInProcessUtilityThread);
  RenderProcessHostImpl::RegisterRendererMainThreadFactory(
      CreateInProcessRendererThread);
  content::RegisterGpuMainThreadFactory(CreateInProcessGpuThread);
}

// Run the main function for browser process.
// Returns the exit code for this process.
int RunBrowserProcessMain(const MainFunctionParams& main_function_params,
                          ContentMainDelegate* delegate) {
  int exit_code = delegate->RunProcess("", main_function_params);
  if (exit_code >= 0)
    return exit_code;
  return BrowserMain(main_function_params);
}

// Run the FooMain() for a given process type.
// Returns the exit code for this process.
int RunOtherNamedProcessTypeMain(const std::string& process_type,
                                 const MainFunctionParams& main_function_params,
                                 ContentMainDelegate* delegate) {
  static const MainFunction kMainFunctions[] = {
#if BUILDFLAG(ENABLE_PLUGINS)
    {switches::kPpapiPluginProcess, PpapiPluginMain},
    {switches::kPpapiBrokerProcess, PpapiBrokerMain},
#endif  // ENABLE_PLUGINS
    {switches::kUtilityProcess, UtilityMain},
    {switches::kRendererProcess, RendererMain},
    {switches::kGpuProcess, GpuMain},
  };

  for (size_t i = 0; i < base::size(kMainFunctions); ++i) {
    if (process_type == kMainFunctions[i].name) {
      int exit_code = delegate->RunProcess(process_type, main_function_params);
      if (exit_code >= 0)
        return exit_code;
      return kMainFunctions[i].function(main_function_params);
    }
  }

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // Zygote startup is special -- see RunZygote comments above
  // for why we don't use ZygoteMain directly.
  if (process_type == switches::kZygoteProcess)
    return RunZygote(delegate);
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

  // If it's a process we don't know about, the embedder should know.
  return delegate->RunProcess(process_type, main_function_params);
}

// static
std::unique_ptr<ContentMainRunnerImpl> ContentMainRunnerImpl::Create() {
  return std::make_unique<ContentMainRunnerImpl>();
}

ContentMainRunnerImpl::ContentMainRunnerImpl() {
#if defined(OS_WIN)
  memset(&sandbox_info_, 0, sizeof(sandbox_info_));
#endif
}

ContentMainRunnerImpl::~ContentMainRunnerImpl() {
  if (is_initialized_ && !is_shutdown_)
    Shutdown();
}

int ContentMainRunnerImpl::TerminateForFatalInitializationError() {
  return delegate_->TerminateForFatalInitializationError();
}

int ContentMainRunnerImpl::Initialize(const ContentMainParams& params) {
  ui_task_ = params.ui_task;
  created_main_parts_closure_ = params.created_main_parts_closure;

#if defined(OS_WIN)
  sandbox_info_ = *params.sandbox_info;
#else  // !OS_WIN

#if defined(OS_MAC)
  autorelease_pool_ = params.autorelease_pool;
#endif  // defined(OS_MAC)

#if defined(OS_ANDROID)
  // Now that mojo's core is initialized (by service manager's Main()), we can
  // enable tracing. Note that only Android builds have the ctor/dtor handlers
  // set up to use trace events at this point (because AtExitManager is already
  // set up when the library is loaded). Other platforms enable tracing below,
  // after the initialization of AtExitManager.
  tracing::EnableStartupTracingIfNeeded();

  TRACE_EVENT0("startup,benchmark,rail", "ContentMainRunnerImpl::Initialize");
#endif  // OS_ANDROID

  base::GlobalDescriptors* g_fds = base::GlobalDescriptors::GetInstance();
  ALLOW_UNUSED_LOCAL(g_fds);

// On Android, the ipc_fd is passed through the Java service.
#if !defined(OS_ANDROID)
  g_fds->Set(kMojoIPCChannel,
             kMojoIPCChannel + base::GlobalDescriptors::kBaseDescriptor);

  g_fds->Set(kFieldTrialDescriptor,
             kFieldTrialDescriptor + base::GlobalDescriptors::kBaseDescriptor);
#endif  // !OS_ANDROID

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_OPENBSD)
  g_fds->Set(kCrashDumpSignal,
             kCrashDumpSignal + base::GlobalDescriptors::kBaseDescriptor);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_OPENBSD)

#endif  // !OS_WIN

  is_initialized_ = true;
  delegate_ = params.delegate;

// The exit manager is in charge of calling the dtors of singleton objects.
// On Android, AtExitManager is set up when library is loaded.
// A consequence of this is that you can't use the ctor/dtor-based
// TRACE_EVENT methods on Linux or iOS builds till after we set this up.
#if !defined(OS_ANDROID)
  if (!ui_task_) {
    // When running browser tests, don't create a second AtExitManager as that
    // interfers with shutdown when objects created before ContentMain is
    // called are destructed when it returns.
    exit_manager_.reset(new base::AtExitManager);
  }
#endif  // !OS_ANDROID

  int exit_code = 0;
  if (!GetContentClient())
    ContentClientCreator::Create(delegate_);
  if (delegate_->BasicStartupComplete(&exit_code))
    return exit_code;
  completed_basic_startup_ = true;

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);

#if defined(OS_WIN)
  if (command_line.HasSwitch(switches::kDeviceScaleFactor)) {
    std::string scale_factor_string =
        command_line.GetSwitchValueASCII(switches::kDeviceScaleFactor);
    double scale_factor = 0;
    if (base::StringToDouble(scale_factor_string, &scale_factor))
      display::win::SetDefaultDeviceScaleFactor(scale_factor);
  }
#endif

  RegisterContentSchemes();
  ContentClientInitializer::Set(process_type, delegate_);

#if !defined(OS_ANDROID)
  // Enable startup tracing asap to avoid early TRACE_EVENT calls being
  // ignored. For Android, startup tracing is enabled in an even earlier place
  // above.
  //
  // Startup tracing flags are not (and should not be) passed to Zygote
  // processes. We will enable tracing when forked, if needed.
  bool enable_startup_tracing = process_type != switches::kZygoteProcess;
#if BUILDFLAG(USE_ZYGOTE_HANDLE)
  // In the browser process, we have to enable startup tracing after
  // InitializeZygoteSandboxForBrowserProcess() is run below, because that
  // function forks and may call trace macros in the forked process.
  if (process_type.empty())
    enable_startup_tracing = false;
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)
  if (enable_startup_tracing)
    tracing::EnableStartupTracingIfNeeded();

#if defined(OS_WIN)
  base::trace_event::TraceEventETWExport::EnableETWExport();
#endif  // OS_WIN

  // Android tracing started at the beginning of the method.
  // Other OSes have to wait till we get here in order for all the memory
  // management setup to be completed.
  TRACE_EVENT0("startup,benchmark,rail", "ContentMainRunnerImpl::Initialize");
#endif  // !OS_ANDROID

    // If we are on a platform where the default allocator is overridden (shim
    // layer on windows, tcmalloc on Linux Desktop) smoke-tests that the
    // overriding logic is working correctly. If not causes a hard crash, as its
    // unexpected absence has security implications.
    CHECK(base::allocator::IsAllocatorInitialized());

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
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

#if defined(OS_ANDROID) && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)
    // On Android, we have two ICU data files. A main one with most languages
    // that is expected to always be available and an extra one that is
    // installed separately via a dynamic feature module. If the extra ICU data
    // file is available we have to apply it _before_ the main ICU data file.
    // Otherwise, the languages of the extra ICU file will be overridden.
    if (process_type.empty()) {
      TRACE_EVENT0("startup", "InitializeICU");
      // In browser process load ICU data files from disk.
      if (GetContentClient()->browser()->ShouldLoadExtraIcuDataFile()) {
        if (!base::i18n::InitializeExtraICU()) {
          return TerminateForFatalInitializationError();
        }
      }
      if (!base::i18n::InitializeICU()) {
        return TerminateForFatalInitializationError();
      }
    } else {
      // In child process map ICU data files loaded by browser process.
      int icu_extra_data_fd = g_fds->MaybeGet(kAndroidICUExtraDataDescriptor);
      if (icu_extra_data_fd != -1) {
        auto icu_extra_data_region =
            g_fds->GetRegion(kAndroidICUExtraDataDescriptor);
        if (!base::i18n::InitializeExtraICUWithFileDescriptor(
                icu_extra_data_fd, icu_extra_data_region)) {
          return TerminateForFatalInitializationError();
        }
      }
      int icu_data_fd = g_fds->MaybeGet(kAndroidICUDataDescriptor);
      if (icu_data_fd == -1) {
        return TerminateForFatalInitializationError();
      }
      auto icu_data_region = g_fds->GetRegion(kAndroidICUDataDescriptor);
      if (!base::i18n::InitializeICUWithFileDescriptor(icu_data_fd,
                                                       icu_data_region)) {
        return TerminateForFatalInitializationError();
      }
    }
#else
    if (!base::i18n::InitializeICU())
      return TerminateForFatalInitializationError();
#endif  // OS_ANDROID && (ICU_UTIL_DATA_IMPL == ICU_UTIL_DATA_FILE)

    InitializeV8IfNeeded(command_line, process_type);

    blink::TrialTokenValidator::SetOriginTrialPolicyGetter(
        base::BindRepeating([]() -> blink::OriginTrialPolicy* {
          if (auto* client = GetContentClient())
            return client->GetOriginTrialPolicy();
          return nullptr;
        }));

#if !defined(OFFICIAL_BUILD)
#if defined(OS_WIN)
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

#if defined(OS_WIN)
    if (!InitializeSandbox(
            sandbox::policy::SandboxTypeFromCommandLine(command_line),
            params.sandbox_info))
      return TerminateForFatalInitializationError();
#elif defined(OS_MAC)
    // Only the GPU process still runs the V1 sandbox.
    bool v2_enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
        sandbox::switches::kSeatbeltClientName);

    if (!v2_enabled && process_type == switches::kGpuProcess) {
      if (!InitializeSandbox()) {
        return TerminateForFatalInitializationError();
      }
    } else if (v2_enabled) {
      CHECK(sandbox::Seatbelt::IsSandboxed());
    }
#endif

    delegate_->SandboxInitialized(process_type);

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
    if (process_type.empty()) {
      // The sandbox host needs to be initialized before forking a thread to
      // start the ServiceManager, and after setting up the sandbox and invoking
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
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

    // Return -1 to indicate no early termination.
    return -1;
}

int ContentMainRunnerImpl::Run(bool start_service_manager_only) {
  DCHECK(is_initialized_);
  DCHECK(!is_shutdown_);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  // Run this logic on all child processes.
  if (!process_type.empty()) {
    if (process_type != switches::kZygoteProcess) {
      // Zygotes will run this at a later point in time when the command line
      // has been updated.
      InitializeFieldTrialAndFeatureList();
      delegate_->PostFieldTrialInitialization();
    }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    // If dynamic Mojo Core is being used, ensure that it's loaded very early in
    // the child/zygote process, before any sandbox is initialized. The library
    // is not fully initialized with IPC support until a ChildProcess is later
    // constructed, as initialization spawns a background thread which would be
    // unsafe here.
    if (IsMojoCoreSharedLibraryEnabled()) {
      CHECK_EQ(mojo::LoadCoreLibrary(GetMojoCoreSharedLibraryPath()),
               MOJO_RESULT_OK);
    }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
  }

  MainFunctionParams main_params(command_line);
  main_params.ui_task = ui_task_;
  main_params.created_main_parts_closure = created_main_parts_closure_;
#if defined(OS_WIN)
  main_params.sandbox_info = &sandbox_info_;
#elif defined(OS_MAC)
  main_params.autorelease_pool = autorelease_pool_;
#endif

  RegisterMainThreadFactories();

  if (process_type.empty())
    return RunServiceManager(main_params, start_service_manager_only);

  return RunOtherNamedProcessTypeMain(process_type, main_params, delegate_);
}

int ContentMainRunnerImpl::RunServiceManager(MainFunctionParams& main_params,
                                             bool start_service_manager_only) {
  TRACE_EVENT_INSTANT0("startup",
                       "ContentMainRunnerImpl::RunServiceManager (begin)",
                       TRACE_EVENT_SCOPE_THREAD);
  if (is_browser_main_loop_started_)
    return -1;

  bool should_start_service_manager_only = start_service_manager_only;
  if (!service_manager_environment_) {
    if (delegate_->ShouldCreateFeatureList()) {
      // This is intentionally leaked since it needs to live for the duration
      // of the process and there's no benefit in cleaning it up at exit.
      base::FieldTrialList* leaked_field_trial_list =
          SetUpFieldTrialsAndFeatureList().release();
      ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
      ignore_result(leaked_field_trial_list);
      delegate_->PostFieldTrialInitialization();
    }

    if (GetContentClient()->browser()->ShouldCreateThreadPool()) {
      // Create and start the ThreadPool early to allow upcoming code to use
      // the post_task.h API.
      base::ThreadPoolInstance::Create("Browser");
    }

    delegate_->PreCreateMainMessageLoop();
#if defined(OS_WIN)
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
    // as BrowserThread::UI in BrowserMainLoop::MainMessageLoopStart).
    BrowserTaskExecutor::Create();

    delegate_->PostEarlyInitialization(main_params.ui_task != nullptr);

    // The hang watcher needs to be started once the feature list is available
    // but before the IO thread is started.
    if (base::HangWatcher::IsEnabled()) {
      hang_watcher_ = new base::HangWatcher();
      hang_watcher_->Start();
      ANNOTATE_LEAKING_OBJECT_PTR(hang_watcher_);
    }

    if (GetContentClient()->browser()->ShouldCreateThreadPool()) {
      // The FeatureList needs to create before starting the ThreadPool.
      StartBrowserThreadPool();
    }

    BrowserTaskExecutor::PostFeatureListSetup();

    tracing::InitTracingPostThreadPoolStartAndFeatureList();

#if defined(OS_ANDROID)
    SetupCpuTimeMetrics();
    // For child processes, this requires allowing of the
    // sched_setaffinity() syscall in the sandbox (baseline_policy_android.cc).
    // When this call is removed, the sandbox allowlist should be updated too.
    SetupCpuAffinityPollingOnce();
#endif

    if (should_start_service_manager_only)
      ForceInProcessNetworkService(true);

    discardable_shared_memory_manager_ =
        std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();

    // PowerMonitor is needed in reduced mode. BrowserMainLoop will safely skip
    // initializing it again if it has already been initialized.
    base::PowerMonitor::Initialize(
        std::make_unique<base::PowerMonitorDeviceSource>());

    service_manager_environment_ = std::make_unique<ServiceManagerEnvironment>(
        BrowserTaskExecutor::CreateIOThread());

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (mojo::PlatformChannel::CommandLineHasPassedEndpoint(command_line)) {
      mojo::PlatformChannelEndpoint endpoint =
          mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
              command_line);
      auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
      GetContentClient()->browser()->BindBrowserControlInterface(
          invitation.ExtractMessagePipe(0));
    }

    download::SetIOTaskRunner(
        service_manager_environment_->io_thread()->task_runner());

    InitializeBrowserMemoryInstrumentationClient();

#if defined(OS_ANDROID)
    if (start_service_manager_only) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&ServiceManagerStartupComplete));
    }
#endif
  }

  if (should_start_service_manager_only) {
    DVLOG(0) << "Chrome is running in ServiceManager only mode.";
    return -1;
  }

  DVLOG(0) << "Chrome is running in full browser mode.";
  is_browser_main_loop_started_ = true;
  startup_data_ = service_manager_environment_->CreateBrowserStartupData();
  main_params.startup_data = startup_data_.get();
  return RunBrowserProcessMain(main_params, delegate_);
}

void ContentMainRunnerImpl::Shutdown() {
  DCHECK(is_initialized_);
  DCHECK(!is_shutdown_);

  service_manager_environment_.reset();

  if (completed_basic_startup_) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    std::string process_type =
        command_line.GetSwitchValueASCII(switches::kProcessType);

    delegate_->ProcessExiting(process_type);
  }

  service_manager_environment_.reset();
  // The BrowserTaskExecutor needs to be destroyed before |exit_manager_|.
  BrowserTaskExecutor::Shutdown();

#if defined(OS_WIN)
#ifdef _CRTDBG_MAP_ALLOC
  _CrtDumpMemoryLeaks();
#endif  // _CRTDBG_MAP_ALLOC
#endif  // OS_WIN

  exit_manager_.reset(nullptr);

  delegate_ = nullptr;
  is_shutdown_ = true;
}

// static
std::unique_ptr<ContentMainRunner> ContentMainRunner::Create() {
  return ContentMainRunnerImpl::Create();
}

}  // namespace content
