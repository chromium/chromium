// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_main_delegate.h"

#import <variant>

#import "content/public/app/initialize_mojo_core.h"
#import "content/public/renderer/content_renderer_client.h"
#import "ios/web/content/init/ios_content_browser_client.h"
#import "ios/web/content/init/ios_content_client.h"
#import "ios/web/content/init/ios_content_renderer_client.h"
#import "ios/web/public/web_client.h"

namespace web {

IOSMainDelegate::IOSMainDelegate() {}
IOSMainDelegate::~IOSMainDelegate() {}

bool IOSMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  // The //content layer is always responsible for creating the FeatureList in
  // child processes.
  if (std::holds_alternative<InvokedInChildProcess>(invoked_in)) {
    return true;
  }

  // Otherwise, normally the browser process in Chrome is responsible for
  // creating the FeatureList.
  return false;
}

bool IOSMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

std::optional<int> IOSMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
#if BUILDFLAG(USE_BLINK)
  web::GetWebClient()->InitializeFieldTrialAndFeatureList();
  content::InitializeMojoCore();
#endif
  return std::nullopt;
}

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

std::variant<int, content::MainFunctionParams> IOSMainDelegate::RunProcess(
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
