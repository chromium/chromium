// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/browser_main_parts.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"

namespace display {
class Screen;
}

namespace content {
struct MainFunctionParams;
}

class WebEngineBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit WebEngineBrowserMainParts(
      const content::MainFunctionParams& parameters,
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ~WebEngineBrowserMainParts() override;

  content::BrowserContext* browser_context() const {
    return browser_context_.get();
  }
  WebEngineDevToolsController* devtools_controller() const {
    return devtools_controller_.get();
  }

  // content::BrowserMainParts overrides.
  void PreMainMessageLoopRun() override;
  void PreDefaultMainMessageLoopRun(base::OnceClosure quit_closure) override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;

  ContextImpl* context_for_test() const { return context_service_.get(); }

 private:
  const content::MainFunctionParams& parameters_;

  fidl::InterfaceRequest<fuchsia::web::Context> request_;

  std::unique_ptr<display::Screen> screen_;
  std::unique_ptr<WebEngineBrowserContext> browser_context_;
  std::unique_ptr<ContextImpl> context_service_;
  std::unique_ptr<fidl::Binding<fuchsia::web::Context>> context_binding_;
  std::unique_ptr<WebEngineDevToolsController> devtools_controller_;

  bool run_message_loop_ = true;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineBrowserMainParts);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_MAIN_PARTS_H_
