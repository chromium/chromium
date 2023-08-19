// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_test_with_web_controller.h"

#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {

WebTestWithWebController::WebTestWithWebController() {}

WebTestWithWebController::WebTestWithWebController(
    std::unique_ptr<web::WebClient> web_client)
    : WebTestWithWebState(std::move(web_client)) {}

WebTestWithWebController::~WebTestWithWebController() {}

void WebTestWithWebController::SetUp() {
  WebTestWithWebState::SetUp();

  // WebViewConfiguration must be configured before triggering any navigations.
  // Otherwise, the JavaScriptContentWorld instances will never be created,
  // triggering a crash on navigation. In Chrome, this happens in
  // `WKWebViewConfigurationProvider::UpdateScripts`, but that method is often
  // not called for tests using this fixture class due to fake and mock classes.
  WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState())
      .GetWebViewConfiguration();
}

CRWWebController* WebTestWithWebController::web_controller() {
  if (!web_state()) {
    return nullptr;
  }
  return web::WebStateImpl::FromWebState(web_state())->GetWebController();
}

}  // namespace web
