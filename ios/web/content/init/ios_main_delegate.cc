// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/content/init/ios_main_delegate.h"

#include "content/public/renderer/content_renderer_client.h"
#include "ios/web/content/init/ios_content_browser_client.h"
#include "ios/web/content/init/ios_content_client.h"
#include "ios/web/content/init/ios_content_renderer_client.h"

namespace web {

IOSMainDelegate::IOSMainDelegate() {}
IOSMainDelegate::~IOSMainDelegate() {}

content::ContentClient* IOSMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<IOSContentClient>();
  return content_client_.get();
}
content::ContentBrowserClient* IOSMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<IOSContentBrowserClient>();
  return browser_client_.get();
}
content::ContentRendererClient* IOSMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<IOSContentRendererClient>();
  return renderer_client_.get();
}

absl::variant<int, content::MainFunctionParams> IOSMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  CHECK_EQ(process_type, "");
  browser_runner_ = content::BrowserMainRunner::Create();

  int exit_code = browser_runner_->Initialize(std::move(main_function_params));
  if (exit_code > 0) {
    return exit_code;
  }
  return 0;
}

}  // namespace web
