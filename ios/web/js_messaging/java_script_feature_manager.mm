// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key used to associate a JavaScriptFeatureManager instances with a
// BrowserState.
const char kWebJavaScriptFeatureManagerKeyName[] =
    "web_java_script_feature_manager";

// Adds common features to |world|.
void AddSharedCommonFeatures(web::JavaScriptContentWorld* world) {
  // The scripts defined by these features were previously hardcoded into
  // js_compile.gni and are assumed to always exist by other feature javascript
  // (regardless of content world).
  // TODO(crbug.com/1152112): Remove unconditional injection of these features
  // once dependent features are migrated to JavaScriptFeatures and correctly
  // define their dependencies.
  world->AddFeature(web::java_script_features::GetBaseJavaScriptFeature());
  world->AddFeature(web::java_script_features::GetCommonJavaScriptFeature());
  world->AddFeature(web::java_script_features::GetMessageJavaScriptFeature());
}

}  // namespace

namespace web {

JavaScriptFeatureManager::JavaScriptFeatureManager(BrowserState* browser_state)
    : browser_state_(browser_state) {}
JavaScriptFeatureManager::~JavaScriptFeatureManager() {}

JavaScriptFeatureManager* JavaScriptFeatureManager::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  JavaScriptFeatureManager* feature_manager =
      static_cast<JavaScriptFeatureManager*>(
          browser_state->GetUserData(kWebJavaScriptFeatureManagerKeyName));
  if (!feature_manager) {
    feature_manager = new JavaScriptFeatureManager(browser_state);
    browser_state->SetUserData(kWebJavaScriptFeatureManagerKeyName,
                               base::WrapUnique(feature_manager));
  }
  return feature_manager;
}

void JavaScriptFeatureManager::ConfigureFeatures(
    std::vector<JavaScriptFeature*> features) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state_);
  // Fetch the WKUserContentController each time features are configured as it
  // is not guarentted to remain constant. (For example, clearing Browsing Data
  // changes the user content controller instance.)
  WKUserContentController* user_content_controller =
      configuration_provider.GetWebViewConfiguration().userContentController;

  page_content_world_ =
      std::make_unique<JavaScriptContentWorld>(user_content_controller);
  AddSharedCommonFeatures(page_content_world_.get());

#if defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_0
  if (@available(iOS 14, *)) {
    isolated_world_ = std::make_unique<JavaScriptContentWorld>(
        user_content_controller, WKContentWorld.defaultClientWorld);
    AddSharedCommonFeatures(isolated_world_.get());
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

JavaScriptContentWorld* JavaScriptFeatureManager::GetContentWorldForFeature(
    JavaScriptFeature* feature) {
  if (isolated_world_ && isolated_world_->HasFeature(feature)) {
    return isolated_world_.get();
  }
  if (page_content_world_->HasFeature(feature)) {
    return page_content_world_.get();
  }
  return nullptr;
}

}  // namespace web
