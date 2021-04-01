// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/browser_main_parts.h"
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
struct MainFunctionParams;
}

namespace cr_fuchsia {
class LegacyMetricsClient;
}

class MediaResourceProviderService;

class WEB_ENGINE_EXPORT WebEngineBrowserMainParts
    : public content::BrowserMainParts {
 public:
  explicit WebEngineBrowserMainParts(
      content::ContentBrowserClient* browser_client,
      const content::MainFunctionParams& parameters,
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ~WebEngineBrowserMainParts() override;

  std::vector<content::BrowserContext*> browser_contexts() const;
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_.get();
  }
  MediaResourceProviderService* media_resource_provider_service() const {
    return media_resource_provider_service_.get();
  }

  // content::BrowserMainParts overrides.
  void PostEarlyInitialization() override;
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  ContextImpl* context_for_test() const;

 private:
  void OnIntlProfileChanged(const fuchsia::intl::Profile& profile);

  content::ContentBrowserClient* const browser_client_;
  const content::MainFunctionParams& parameters_;

  fidl::InterfaceRequest<fuchsia::web::Context> request_;

  std::unique_ptr<display::Screen> screen_;
  fidl::BindingSet<fuchsia::web::Context, std::unique_ptr<ContextImpl>>
      context_bindings_;
  std::unique_ptr<WebEngineDevToolsController> devtools_controller_;
  std::unique_ptr<cr_fuchsia::LegacyMetricsClient> legacy_metrics_client_;
  std::unique_ptr<MediaResourceProviderService>
      media_resource_provider_service_;

  // Used to respond to changes to the system's current locale.
  std::unique_ptr<base::FuchsiaIntlProfileWatcher> intl_profile_watcher_;

  bool run_message_loop_ = true;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineBrowserMainParts);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
