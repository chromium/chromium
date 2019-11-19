// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_WEB_ENGINE_MAIN_DELEGATE_H_
#define FUCHSIA_ENGINE_WEB_ENGINE_MAIN_DELEGATE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/zx/channel.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/app/content_main_delegate.h"
#include "fuchsia/engine/web_engine_export.h"

namespace content {
class ContentClient;
}  // namespace content

class WebEngineContentBrowserClient;
class WebEngineContentRendererClient;

class WEB_ENGINE_EXPORT WebEngineMainDelegate
    : public content::ContentMainDelegate {
 public:
  explicit WebEngineMainDelegate(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ~WebEngineMainDelegate() override;

  static WebEngineMainDelegate* GetInstanceForTest();

  WebEngineContentBrowserClient* browser_client() {
    return browser_client_.get();
  }

  // ContentMainDelegate implementation.
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<WebEngineContentBrowserClient> browser_client_;
  std::unique_ptr<WebEngineContentRendererClient> renderer_client_;

  fidl::InterfaceRequest<fuchsia::web::Context> request_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineMainDelegate);
};

#endif  // FUCHSIA_ENGINE_WEB_ENGINE_MAIN_DELEGATE_H_
