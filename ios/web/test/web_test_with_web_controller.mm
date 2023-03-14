// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_test_with_web_controller.h"

#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebTestWithWebController::WebTestWithWebController() {}

WebTestWithWebController::WebTestWithWebController(
    std::unique_ptr<web::WebClient> web_client)
    : WebTestWithWebState(std::move(web_client)) {}

WebTestWithWebController::~WebTestWithWebController() {}

void WebTestWithWebController::SetUp() {
  WebTestWithWebState::SetUp();

  JavaScriptFeatureManager* java_script_feature_manager =
      JavaScriptFeatureManager::FromBrowserState(GetBrowserState());
  // Features must be configured before triggering any navigations. Otherwise,
  // the JavaScriptContentWorld instances will never be created, triggering a
  // crash on navigation. In Chrome, this happens in
  // `WKWebViewConfigurationProvider::UpdateScripts`, but that method is often
  // not called for tests using this fixture class due to fake and mock classes.
  java_script_feature_manager->ConfigureFeatures({});
}

CRWWebController* WebTestWithWebController::web_controller() {
  if (!web_state()) {
    return nullptr;
  }
  return web::WebStateImpl::FromWebState(web_state())->GetWebController();
}

}  // namespace web
