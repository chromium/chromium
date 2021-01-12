// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key used to associate a JavaScriptFeatureManager instances with a
// BrowserState.
const char kWebJavaScriptFeatureManagerKeyName[] =
    "web_java_script_feature_manager";

}  // namespace

namespace web {

JavaScriptFeatureManager::JavaScriptFeatureManager(
    WKUserContentController* user_content_controller)
    : user_content_controller_(user_content_controller) {}
JavaScriptFeatureManager::~JavaScriptFeatureManager() {}

JavaScriptFeatureManager* JavaScriptFeatureManager::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  JavaScriptFeatureManager* feature_manager =
      static_cast<JavaScriptFeatureManager*>(
          browser_state->GetUserData(kWebJavaScriptFeatureManagerKeyName));
  if (!feature_manager) {
    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(browser_state);
    WKUserContentController* user_content_controller =
        configuration_provider.GetWebViewConfiguration().userContentController;
    feature_manager = new JavaScriptFeatureManager(user_content_controller);
    browser_state->SetUserData(kWebJavaScriptFeatureManagerKeyName,
                               base::WrapUnique(feature_manager));
  }
  return feature_manager;
}

void JavaScriptFeatureManager::ConfigureFeatures(
    std::vector<JavaScriptFeature*> features) {
  page_content_world_ =
      std::make_unique<JavaScriptContentWorld>(user_content_controller_);

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    isolated_world_ = std::make_unique<JavaScriptContentWorld>(
        user_content_controller_, WKContentWorld.defaultClientWorld);
  }
#endif  // defined(__IPHONE14_0)

  for (JavaScriptFeature* feature : features) {
    if (isolated_world_ &&
        feature->GetSupportedContentWorld() ==
            JavaScriptFeature::ContentWorld::kAnyContentWorld) {
      isolated_world_->AddFeature(feature);
    } else {
      page_content_world_->AddFeature(feature);
    }
  }
}

}  // namespace web
