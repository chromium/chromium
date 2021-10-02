// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/inspect/cpp/component.h>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/process_context.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "fuchsia/base/inspect.h"
#include "fuchsia/base/legacymetrics_client.h"
#include "fuchsia/engine/browser/cdm_provider_service.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/browser/web_engine_memory_inspector.h"
#include "fuchsia/engine/common/cast_streaming.h"
#include "fuchsia/engine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/aura/screen_ozone.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

base::NoDestructor<fidl::InterfaceRequest<fuchsia::web::Context>>
    g_test_request;

constexpr base::TimeDelta kMetricsReportingInterval = base::Minutes(1);

constexpr base::TimeDelta kChildProcessHistogramFetchTimeout =
    base::Seconds(10);

// Merge child process' histogram deltas into the browser process' histograms.
void FetchHistogramsFromChildProcesses(
    base::OnceCallback<void(std::vector<fuchsia::legacymetrics::Event>)>
        done_cb) {
  content::FetchHistogramsAsynchronously(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(std::move(done_cb),
                     std::vector<fuchsia::legacymetrics::Event>()),
      kChildProcessHistogramFetchTimeout);
}

// Implements the fuchsia.web.FrameHost protocol using a ContextImpl with
// incognito browser context.
class FrameHostImpl final : public fuchsia::web::FrameHost {
 public:
  explicit FrameHostImpl(inspect::Node inspect_node,
                         WebEngineDevToolsController* devtools_controller)
      : context_(WebEngineBrowserContext::CreateIncognito(),
                 std::move(inspect_node),
                 devtools_controller) {}
  ~FrameHostImpl() override = default;

  FrameHostImpl(const FrameHostImpl&) = delete;
  FrameHostImpl& operator=(const FrameHostImpl&) = delete;

  // fuchsia.web.FrameHost implementation.
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> request) override {
    context_.CreateFrameWithParams(std::move(params), std::move(request));
  }

 private:
  ContextImpl context_;
};

}  // namespace

WebEngineBrowserMainParts::WebEngineBrowserMainParts(
    content::ContentBrowserClient* browser_client,
    const content::MainFunctionParams& parameters)
    : browser_client_(browser_client), parameters_(parameters) {}

WebEngineBrowserMainParts::~WebEngineBrowserMainParts() {
  display::Screen::SetScreenInstance(nullptr);
}

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
  DCHECK(!screen_);
  DCHECK_EQ(context_bindings_.size(), 0u);

  // Initialize the |component_inspector_| to allow diagnostics to be published.
  component_inspector_ = std::make_unique<sys::ComponentInspector>(
      base::ComponentContextForProcess());
  cr_fuchsia::PublishVersionInfoToInspect(component_inspector_.get());

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

  if (command_line->HasSwitch(switches::kUseLegacyMetricsService)) {
    legacy_metrics_client_ =
        std::make_unique<cr_fuchsia::LegacyMetricsClient>();

    // Add a hook to asynchronously pull metrics from child processes just prior
    // to uploading.
    legacy_metrics_client_->SetReportAdditionalMetricsCallback(
        base::BindRepeating(&FetchHistogramsFromChildProcesses));

    legacy_metrics_client_->Start(kMetricsReportingInterval);
  }

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
  std::unique_ptr<aura::ScreenOzone> screen_ozone =
      std::make_unique<aura::ScreenOzone>();
  screen_ozone.get()->Initialize();
  screen_ = std::move(screen_ozone);
  display::Screen::SetScreenInstance(screen_.get());

  // Create the CdmProviderService at startup rather than on-demand,
  // to allow it to perform potentially expensive startup work in the
  // background.
  cdm_provider_service_ = std::make_unique<CdmProviderService>();

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

  // Now that all services have been published, it is safe to start processing
  // requests to the service directory.
  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // TODO(crbug.com/1163073): Update tests to make a service connection to the
  // Context and remove this workaround.
  if (*g_test_request)
    HandleContextRequest(std::move(*g_test_request));

  // In browser tests |ui_task| runs the "body" of each test.
  if (parameters_.ui_task) {
    // Since the main loop won't run, there is nothing to quit.
    quit_closure_ = base::DoNothing();

    std::move(*parameters_.ui_task).Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

void WebEngineBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  if (run_message_loop_)
    quit_closure_ = run_loop->QuitClosure();
  else
    run_loop.reset();
}

void WebEngineBrowserMainParts::PostMainMessageLoopRun() {
  // Main loop should quit only after all Context instances have been destroyed.
  DCHECK_EQ(context_bindings_.size(), 0u);

  // These resources must be freed while a MessageLoop is still available, so
  // that they may post cleanup tasks during teardown.
  // NOTE: Objects are destroyed in the reverse order of their creation.
  legacy_metrics_client_.reset();
  screen_.reset();
  intl_profile_watcher_.reset();

  base::ImportantFileWriterCleaner::GetInstance().Stop();
}

// static
void WebEngineBrowserMainParts::SetContextRequestForTest(
    fidl::InterfaceRequest<fuchsia::web::Context> request) {
  *g_test_request.get() = std::move(request);
}

ContextImpl* WebEngineBrowserMainParts::context_for_test() const {
  DCHECK_EQ(context_bindings_.size(), 1u);
  return context_bindings_.bindings().front()->impl().get();
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
    browser_context = WebEngineBrowserContext::CreateIncognito();
  } else {
    browser_context = WebEngineBrowserContext::CreatePersistent(
        base::FilePath(base::kPersistedDataDirectoryPath));
  }

  auto inspect_node_name =
      base::StringPrintf("context-%lu", *base::GetKoid(request.channel()));
  auto context_impl = std::make_unique<ContextImpl>(
      std::move(browser_context),
      component_inspector_->root().CreateChild(inspect_node_name),
      devtools_controller_.get());

  // If this web instance should allow CastStreaming then enable it in this
  // ContextImpl. CastStreaming will not be available in FrameHost contexts.
  if (IsCastStreamingEnabled())
    context_impl->SetCastStreamingEnabled();

  // Create the fuchsia.web.Context implementation using the BrowserContext and
  // configure it to terminate the process when the client goes away.
  context_bindings_.AddBinding(
      std::move(context_impl), std::move(request), /* dispatcher */ nullptr,
      // Quit the browser main loop when the Context connection is
      // dropped.
      [this](zx_status_t status) {
        ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
            << " Context disconnected.";
        std::move(quit_closure_).Run();
      });
}

void WebEngineBrowserMainParts::HandleFrameHostRequest(
    fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
  auto inspect_node_name =
      base::StringPrintf("framehost-%lu", *base::GetKoid(request.channel()));
  frame_host_bindings_.AddBinding(
      std::make_unique<FrameHostImpl>(
          component_inspector_->root().CreateChild(inspect_node_name),
          devtools_controller_.get()),
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
