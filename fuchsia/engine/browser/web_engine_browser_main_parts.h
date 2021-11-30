// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <string>

#include "base/fuchsia/process_lifecycle.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/common/main_function_params.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/web_engine_export.h"

namespace base {
class FuchsiaIntlProfileWatcher;
}

namespace display {
class Screen;
}

namespace content {
class ContentBrowserClient;
}

namespace cr_fuchsia {
class LegacyMetricsClient;
}

namespace sys {
class ComponentInspector;
}

class CdmProviderService;
class WebEngineMemoryInspector;

class WEB_ENGINE_EXPORT WebEngineBrowserMainParts
    : public content::BrowserMainParts {
 public:
  WebEngineBrowserMainParts(content::ContentBrowserClient* browser_client,
                            content::MainFunctionParams parameters);
  ~WebEngineBrowserMainParts() override;

  WebEngineBrowserMainParts(const WebEngineBrowserMainParts&) = delete;
  WebEngineBrowserMainParts& operator=(const WebEngineBrowserMainParts&) =
      delete;

  std::vector<content::BrowserContext*> browser_contexts() const;
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_.get();
  }
  CdmProviderService* cdm_provider_service() const {
    return cdm_provider_service_.get();
  }

  // content::BrowserMainParts overrides.
  void PostEarlyInitialization() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  // Methods used by tests.
  static void SetContextRequestForTest(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ContextImpl* context_for_test() const;

 private:
  // Handle fuchsia.web.Context and fuchsia.web.FrameHost connection requests.
  void HandleContextRequest(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  void HandleFrameHostRequest(
      fidl::InterfaceRequest<fuchsia::web::FrameHost> request);

  // Notified if the system timezone, language, settings change.
  void OnIntlProfileChanged(const fuchsia::intl::Profile& profile);

  // Quits the main loop and gracefully shuts down the instance.
  void BeginGracefulShutdown();

  content::ContentBrowserClient* const browser_client_;
  content::MainFunctionParams parameters_;

  // Used to gracefully teardown in response to requests from the ELF runner.
  std::unique_ptr<base::ProcessLifecycle> lifecycle_;

  std::unique_ptr<display::Screen> screen_;

  // Used to publish diagnostics including the active Contexts and FrameHosts.
  std::unique_ptr<sys::ComponentInspector> component_inspector_;
  std::unique_ptr<WebEngineMemoryInspector> memory_inspector_;

  // Browsing contexts for the connected clients.
  fidl::BindingSet<fuchsia::web::Context, std::unique_ptr<ContextImpl>>
      context_bindings_;
  fidl::BindingSet<fuchsia::web::FrameHost,
                   std::unique_ptr<fuchsia::web::FrameHost>>
      frame_host_bindings_;

  std::unique_ptr<WebEngineDevToolsController> devtools_controller_;
  std::unique_ptr<cr_fuchsia::LegacyMetricsClient> legacy_metrics_client_;
  std::unique_ptr<CdmProviderService> cdm_provider_service_;

  // Used to respond to changes to the system's current locale.
  std::unique_ptr<base::FuchsiaIntlProfileWatcher> intl_profile_watcher_;

  base::OnceClosure quit_closure_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
