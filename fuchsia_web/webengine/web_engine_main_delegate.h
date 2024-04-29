// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_MAIN_DELEGATE_H_
#define FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_MAIN_DELEGATE_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/zx/channel.h>

#include <memory>
#include <optional>
#include <string>

#include "content/public/app/content_main_delegate.h"
#include "fuchsia_web/webengine/web_engine_export.h"

namespace content {
class ContentClient;
}  // namespace content

class WebEngineContentBrowserClient;
class WebEngineContentRendererClient;

class WEB_ENGINE_EXPORT WebEngineMainDelegate
    : public content::ContentMainDelegate {
 public:
  explicit WebEngineMainDelegate();

  WebEngineMainDelegate(const WebEngineMainDelegate&) = delete;
  WebEngineMainDelegate& operator=(const WebEngineMainDelegate&) = delete;

  ~WebEngineMainDelegate() override;

  WebEngineContentBrowserClient* browser_client() {
    return browser_client_.get();
  }

  // ContentMainDelegate implementation.
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  std::optional<int> PreBrowserMain() override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  static WebEngineMainDelegate* GetInstanceForTest();

 private:
  std::unique_ptr<content::ContentClient> content_client_;
  std::unique_ptr<WebEngineContentBrowserClient> browser_client_;
  std::unique_ptr<WebEngineContentRendererClient> renderer_client_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_WEB_ENGINE_MAIN_DELEGATE_H_
