// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"

#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/important_file_writer_cleaner.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/main_function_params.h"
#include "fuchsia/base/legacymetrics_client.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/media_resource_provider_service.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/aura/screen_ozone.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

constexpr base::TimeDelta kMetricsReportingInterval =
    base::TimeDelta::FromMinutes(1);

constexpr base::TimeDelta kChildProcessHistogramFetchTimeout =
    base::TimeDelta::FromSeconds(10);

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

}  // namespace

WebEngineBrowserMainParts::WebEngineBrowserMainParts(
    const content::MainFunctionParams& parameters,
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : parameters_(parameters), request_(std::move(request)) {
  DCHECK(request_);
}

WebEngineBrowserMainParts::~WebEngineBrowserMainParts() {
  display::Screen::SetScreenInstance(nullptr);
}

void WebEngineBrowserMainParts::PostEarlyInitialization() {
  base::ImportantFileWriterCleaner::GetInstance().Initialize();
}

void WebEngineBrowserMainParts::PreMainMessageLoopRun() {
  DCHECK(!screen_);

  // Watch for changes to the user's locale setting.
  intl_profile_watcher_ = std::make_unique<base::FuchsiaIntlProfileWatcher>(
      base::BindRepeating(&WebEngineBrowserMainParts::OnIntlProfileChanged,
                          base::Unretained(this)));

  screen_ = std::make_unique<aura::ScreenOzone>();
  display::Screen::SetScreenInstance(screen_.get());

  // If Vulkan is not enabled then disable hardware acceleration. Otherwise gpu
  // process will be restarted several times trying to initialize GL before
  // falling back to software compositing.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseVulkan)) {
    content::GpuDataManager* gpu_data_manager =
        content::GpuDataManager::GetInstance();
    DCHECK(gpu_data_manager);
    gpu_data_manager->DisableHardwareAcceleration();
  }

  DCHECK(!browser_context_);
  browser_context_ = std::make_unique<WebEngineBrowserContext>(
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kIncognito));

  devtools_controller_ = WebEngineDevToolsController::CreateFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
  context_service_ = std::make_unique<ContextImpl>(browser_context_.get(),
                                                   devtools_controller_.get());
  context_binding_ = std::make_unique<fidl::Binding<fuchsia::web::Context>>(
      context_service_.get(), std::move(request_));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseLegacyMetricsService)) {
    legacy_metrics_client_ =
        std::make_unique<cr_fuchsia::LegacyMetricsClient>();

    // Add a hook to asynchronously pull metrics from child processes just prior
    // to uploading.
    legacy_metrics_client_->SetReportAdditionalMetricsCallback(
        base::BindRepeating(&FetchHistogramsFromChildProcesses));

    legacy_metrics_client_->Start(kMetricsReportingInterval);
  }

  // Create the MediaResourceProviderService at startup rather than on-demand,
  // to allow it to perform potentially expensive startup work in the
  // background.
  media_resource_provider_service_ =
      std::make_unique<MediaResourceProviderService>();

  // Quit the browser main loop when the Context connection is dropped.
  context_binding_->set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
        << " Context disconnected.";
    context_service_.reset();
    std::move(quit_closure_).Run();
  });

  // Disable RenderFrameHost's Javascript injection restrictions so that the
  // Context and Frames can implement their own JS injection policy at a higher
  // level.
  content::RenderFrameHost::AllowInjectingJavaScript();

  if (parameters_.ui_task) {
    // Since the main loop won't run, there is nothing to quit in the
    // |context_binding_| error handler.
    quit_closure_ = base::DoNothing::Once();

    std::move(*parameters_.ui_task).Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  }

  // Make sure temporary files associated with this process are cleaned up.
  base::ImportantFileWriterCleaner::GetInstance().Start();
}

void WebEngineBrowserMainParts::PreDefaultMainMessageLoopRun(
    base::OnceClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

bool WebEngineBrowserMainParts::MainMessageLoopRun(int* result_code) {
  return !run_message_loop_;
}

void WebEngineBrowserMainParts::PostMainMessageLoopRun() {
  // The service and its binding should have already been released by the error
  // handler.
  DCHECK(!context_service_);
  DCHECK(!context_binding_->is_bound());

  // These resources must be freed while a MessageLoop is still available, so
  // that they may post cleanup tasks during teardown.
  // NOTE: Please destroy objects in the reverse order of their creation.
  legacy_metrics_client_.reset();
  context_binding_.reset();
  browser_context_.reset();
  screen_.reset();
  intl_profile_watcher_.reset();

  base::ImportantFileWriterCleaner::GetInstance().Stop();
}

void WebEngineBrowserMainParts::OnIntlProfileChanged(
    const fuchsia::intl::Profile& profile) {
  // Configure the ICU library in this process with the new primary locale.
  std::string primary_locale =
      base::FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdFromProfile(profile);
  base::i18n::SetICUDefaultLocale(primary_locale);

  // Reload locale-specific resources.
  std::string loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          base::i18n::GetConfiguredLocale());
  VLOG(1) << "Reloaded locale resources: " << loaded_locale;

  // Reconfigure the network process.
  content::BrowserContext::GetDefaultStoragePartition(browser_context_.get())
      ->GetNetworkContext()
      ->SetAcceptLanguage(net::HttpUtil::GenerateAcceptLanguageHeader(
          browser_context_->GetPreferredLanguages()));
}
