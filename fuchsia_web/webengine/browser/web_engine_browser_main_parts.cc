// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_browser_main_parts.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/inspect/component/cpp/component.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <utility>
#include <vector>

#include <lib/async/default.h>
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/fuchsia_component_support/inspect.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/network_quality_observer_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_context.h"
#include "fuchsia_web/webengine/browser/web_engine_devtools_controller.h"
#include "fuchsia_web/webengine/browser/web_engine_memory_inspector.h"
#include "fuchsia_web/webengine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/mojo/services/fuchsia_cdm_manager.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/aura/screen_ozone.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
#include "components/fuchsia_legacymetrics/legacymetrics_client.h"  // nogncheck
#include "fuchsia_web/webengine/common/cast_streaming.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

namespace {

fidl::InterfaceRequest<fuchsia::web::Context>& GetTestRequest() {
  static base::NoDestructor<fidl::InterfaceRequest<fuchsia::web::Context>>
      request;
  return *request;
}

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
constexpr base::TimeDelta kMetricsReportingInterval = base::Minutes(1);

constexpr base::TimeDelta kChildProcessHistogramFetchTimeout =
    base::Seconds(10);

// Merge child process' histogram deltas into the browser process' histograms.
void FetchHistogramsFromChildProcesses(
    base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
        done_cb) {
  content::FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(std::move(done_cb),
                     std::vector<fuchsia::legacymetrics::Event>()),
      kChildProcessHistogramFetchTimeout);
}
#endif

template <typename KeySystemInterface>
fidl::InterfaceHandle<fuchsia::media::drm::KeySystem> ConnectToKeySystem() {
  static_assert(
      (std::is_same<KeySystemInterface, fuchsia::media::drm::Widevine>::value ||
       std::is_same<KeySystemInterface, fuchsia::media::drm::PlayReady>::value),
      "KeySystemInterface must be either fuchsia::media::drm::Widevine or "
      "fuchsia::media::drm::PlayReady");

  fidl::InterfaceHandle<fuchsia::media::drm::KeySystem> key_system;
  base::ComponentContextForProcess()->svc()->Connect(key_system.NewRequest(),
                                                     KeySystemInterface::Name_);
  return key_system;
}

std::unique_ptr<media::FuchsiaCdmManager> CreateCdmManager() {
  media::FuchsiaCdmManager::CreateKeySystemCallbackMap
      create_key_system_callbacks;

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableWidevine)) {
#if BUILDFLAG(ENABLE_WIDEVINE)
    create_key_system_callbacks.emplace(
        kWidevineKeySystem,
        base::BindRepeating(
            &ConnectToKeySystem<fuchsia::media::drm::Widevine>));
#else
    LOG(WARNING) << "Widevine is not supported.";
#endif
  }

  std::string playready_key_system =
      command_line->GetSwitchValueASCII(switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_CAST_RECEIVER)
    create_key_system_callbacks.emplace(
        playready_key_system,
        base::BindRepeating(
            &ConnectToKeySystem<fuchsia::media::drm::PlayReady>));
#else
    LOG(WARNING) << "PlayReady is not supported.";
#endif
  }

  std::string cdm_data_directory =
      command_line->GetSwitchValueASCII(switches::kCdmDataDirectory);

  std::optional<uint64_t> cdm_data_quota_bytes;
  if (command_line->HasSwitch(switches::kCdmDataQuotaBytes)) {
    uint64_t value = 0;
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(switches::kCdmDataQuotaBytes),
        &value));
    cdm_data_quota_bytes = value;
  }

  return std::make_unique<media::FuchsiaCdmManager>(
      std::move(create_key_system_callbacks),
      base::FilePath(cdm_data_directory), cdm_data_quota_bytes);
}

}  // namespace

void FrameHostImpl::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> request) {
  context_.CreateFrameWithParams(std::move(params), std::move(request));
}

WebEngineBrowserMainParts::WebEngineBrowserMainParts(
    content::ContentBrowserClient* browser_client)
    : browser_client_(browser_client) {}

WebEngineBrowserMainParts::~WebEngineBrowserMainParts() = default;

std::vector<content::BrowserContext*>
WebEngineBrowserMainParts::browser_contexts() const {
  std::vector<content::BrowserContext*> contexts;
  contexts.reserve(context_bindings_.size());
  for (auto& binding : context_bindings_.bindings())
    contexts.push_back(binding->impl()->browser_context());
  return contexts;
}

void WebEngineBrowserMainParts::PostEarlyInitialization() {
  base::ImportantFileWriterCleaner::GetInstance().Initialize();
}

int WebEngineBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK_EQ(context_bindings_.size(), 0u);

  // Initialize the |component_inspector_| to allow diagnostics to be published.
  component_inspector_ = std::make_unique<inspect::ComponentInspector>(
      async_get_default_dispatcher(), inspect::PublishOptions{});
  fuchsia_component_support::PublishVersionInfoToInspect(
      &component_inspector_->root());

  // Add a node providing memory details for this whole web instance.
  memory_inspector_ =
      std::make_unique<WebEngineMemoryInspector>(component_inspector_->root());

  const auto* command_line = base::CommandLine::ForCurrentProcess();

  // If Vulkan is not enabled then disable hardware acceleration. Otherwise gpu
  // process will be restarted several times trying to initialize GL before
  // falling back to software compositing.
  if (!command_line->HasSwitch(switches::kUseVulkan)) {
    content::GpuDataManager* gpu_data_manager =
        content::GpuDataManager::GetInstance();
    DCHECK(gpu_data_manager);
    gpu_data_manager->DisableHardwareAcceleration();
  }

  devtools_controller_ =
      WebEngineDevToolsController::CreateFromCommandLine(*command_line);

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  if (command_line->HasSwitch(switches::kUseLegacyMetricsService)) {
    legacy_metrics_client_ =
        std::make_unique<fuchsia_legacymetrics::LegacyMetricsClient>();

    // Add a hook to asynchronously pull metrics from child processes just prior
    // to uploading.
    legacy_metrics_client_->SetReportAdditionalMetricsCallback(
        base::BindRepeating(&FetchHistogramsFromChildProcesses));

    legacy_metrics_client_->Start(kMetricsReportingInterval);
  }
#endif

  // Configure SysInfo to report total/free space under "/data" based on the
  // requested soft-quota, if any. This only affects persistent instances.
  if (command_line->HasSwitch(switches::kDataQuotaBytes)) {
    // Setting quota on "/data" is benign in incognito contexts, but indicates
    // that the client probably mis-configured this instance.
    DCHECK(!command_line->HasSwitch(switches::kIncognito))
        << "data_quota_bytes set for incognito instance.";

    uint64_t quota_bytes = 0;
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(switches::kDataQuotaBytes),
        &quota_bytes));
    base::SysInfo::SetAmountOfTotalDiskSpace(
        base::FilePath(base::kPersistedDataDirectoryPath), quota_bytes);
  }

  // Watch for changes to the user's locale setting.
  intl_profile_watcher_ = std::make_unique<base::FuchsiaIntlProfileWatcher>(
      base::BindRepeating(&WebEngineBrowserMainParts::OnIntlProfileChanged,
                          base::Unretained(this)));

  // Configure Ozone with an Aura implementation of the Screen abstraction.
  screen_ = std::make_unique<aura::ScreenOzone>();

  // Create the FuchsiaCdmManager at startup rather than on-demand, to allow it
  // to perform potentially expensive startup work in the background.
  cdm_manager_ = CreateCdmManager();

  // Disable RenderFrameHost's Javascript injection restrictions so that the
  // Context and Frames can implement their own JS injection policy at a higher
  // level.
  content::RenderFrameHost::AllowInjectingJavaScript();

  // Make sure temporary files associated with this process are cleaned up.
  base::ImportantFileWriterCleaner::GetInstance().Start();

  // Publish the fuchsia.web.Context and fuchsia.web.FrameHost capabilities.
  base::ComponentContextForProcess()->outgoing()->AddPublicService(
      fidl::InterfaceRequestHandler<fuchsia::web::Context>(fit::bind_member(
          this, &WebEngineBrowserMainParts::HandleContextRequest)));
  base::ComponentContextForProcess()->outgoing()->AddPublicService(
      fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>(fit::bind_member(
          this, &WebEngineBrowserMainParts::HandleFrameHostRequest)));

  // TODO(crbug.com/42050460): Create a base::ProcessLifecycle instance here, to
  // trigger graceful shutdown on component stop, when migrated to CFv2.

  // Manage network-quality signals and send them to renderers. Provides input
  // for networking-related Client Hints.
  network_quality_tracker_ = std::make_unique<network::NetworkQualityTracker>(
      base::BindRepeating(&content::GetNetworkService));
  network_quality_observer_ =
      content::CreateNetworkQualityObserver(network_quality_tracker_.get());

  // Now that all services have been published, it is safe to start processing
  // requests to the service directory.
  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // TODO(crbug.com/40162984): Update tests to make a service connection to the
  // Context and remove this workaround.
  fidl::InterfaceRequest<fuchsia::web::Context>& request = GetTestRequest();
  if (request)
    HandleContextRequest(std::move(request));

  return content::RESULT_CODE_NORMAL_EXIT;
}

void WebEngineBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_closure_ = run_loop->QuitClosure();
}

void WebEngineBrowserMainParts::PostMainMessageLoopRun() {
  // Main loop should quit only after all Context instances have been destroyed.
  DCHECK_EQ(context_bindings_.size(), 0u);

  // FrameHost channels may still be active and contain live Frames. Close them
  // here so that they are torn-down before their dependent resources.
  frame_host_bindings_.CloseAll();

  // These resources must be freed while a MessageLoop is still available, so
  // that they may post cleanup tasks during teardown.
  // NOTE: Objects are destroyed in the reverse order of their creation.
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  legacy_metrics_client_.reset();
#endif
  intl_profile_watcher_.reset();

  base::ImportantFileWriterCleaner::GetInstance().Stop();
}

// static
void WebEngineBrowserMainParts::SetContextRequestForTest(
    fidl::InterfaceRequest<fuchsia::web::Context> request) {
  GetTestRequest() = std::move(request);
}

ContextImpl* WebEngineBrowserMainParts::context_for_test() const {
  if (context_bindings_.size() == 0)
    return nullptr;
  return context_bindings_.bindings().front()->impl().get();
}

std::vector<FrameHostImpl*> WebEngineBrowserMainParts::frame_hosts_for_test()
    const {
  std::vector<FrameHostImpl*> frame_host_impls;
  for (auto& binding : frame_host_bindings_.bindings()) {
    frame_host_impls.push_back(binding->impl().get());
  }
  return frame_host_impls;
}

void WebEngineBrowserMainParts::HandleContextRequest(
    fidl::InterfaceRequest<fuchsia::web::Context> request) {
  if (context_bindings_.size() > 0) {
    request.Close(ZX_ERR_BAD_STATE);
    return;
  }

  // Create the BrowserContext for the fuchsia.web.Context, with persistence
  // configured as requested via the command-line.
  std::unique_ptr<WebEngineBrowserContext> browser_context;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kIncognito)) {
    browser_context = WebEngineBrowserContext::CreateIncognito(
        network_quality_tracker_.get());
  } else {
    browser_context = WebEngineBrowserContext::CreatePersistent(
        base::FilePath(base::kPersistedDataDirectoryPath),
        network_quality_tracker_.get());
  }

  auto inspect_node_name =
      base::StringPrintf("context-%lu", *base::GetKoid(request.channel()));
  auto context_impl = std::make_unique<ContextImpl>(
      std::move(browser_context),
      component_inspector_->root().CreateChild(inspect_node_name),
      devtools_controller_.get());

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  // If this web instance should allow CastStreaming then enable it in this
  // ContextImpl. CastStreaming will not be available in FrameHost contexts.
  if (IsCastStreamingEnabled())
    context_impl->SetCastStreamingEnabled();
#endif

  // Create the fuchsia.web.Context implementation using the BrowserContext and
  // configure it to terminate the process when the client goes away.
  context_bindings_.AddBinding(
      std::move(context_impl), std::move(request), /* dispatcher */ nullptr,
      // Quit the browser main loop when the Context connection is
      // dropped.
      [this](zx_status_t status) {
        ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
            << " Context disconnected.";
        BeginGracefulShutdown();
      });
}

void WebEngineBrowserMainParts::HandleFrameHostRequest(
    fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
  auto inspect_node_name =
      base::StringPrintf("framehost-%lu", *base::GetKoid(request.channel()));
  frame_host_bindings_.AddBinding(
      std::make_unique<FrameHostImpl>(
          component_inspector_->root().CreateChild(inspect_node_name),
          devtools_controller_.get(), network_quality_tracker_.get()),
      std::move(request));
}

void WebEngineBrowserMainParts::OnIntlProfileChanged(
    const fuchsia::intl::Profile& profile) {
  // Configure the ICU library in this process with the new primary locale.
  std::string primary_locale =
      base::FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdFromProfile(profile);
  base::i18n::SetICUDefaultLocale(primary_locale);

  {
    // Reloading locale-specific resources requires synchronous blocking.
    // Locale changes should not be frequent enough for this to cause jank.
    base::ScopedAllowBlocking allow_blocking;

    std::string loaded_locale =
        ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
            base::i18n::GetConfiguredLocale());

    VLOG(1) << "Reloaded locale resources: " << loaded_locale;
  }

  // Reconfigure each web.Context's NetworkContext with the new setting.
  for (auto& binding : context_bindings_.bindings()) {
    content::BrowserContext* const browser_context =
        binding->impl()->browser_context();
    std::string accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
        browser_client_->GetAcceptLangs(browser_context));
    browser_context->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->SetAcceptLanguage(accept_language);
  }
}

void WebEngineBrowserMainParts::BeginGracefulShutdown() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}
