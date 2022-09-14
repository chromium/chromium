// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#import "base/ios/ios_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Key used to associate a JavaScriptFeatureManager instances with a
// BrowserState.
const char kWebJavaScriptFeatureManagerKeyName[] =
    "web_java_script_feature_manager";

// Adds common features to `world`.
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

JavaScriptContentWorld*
JavaScriptFeatureManager::GetPageContentWorldForBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);
  JavaScriptFeatureManager* feature_manager = FromBrowserState(browser_state);
  return feature_manager->page_content_world_.get();
}

void JavaScriptFeatureManager::ConfigureFeatures(
    std::vector<JavaScriptFeature*> features) {
  page_content_world_ = std::make_unique<JavaScriptContentWorld>(
      browser_state_, WKContentWorld.pageWorld);
  AddSharedCommonFeatures(page_content_world_.get());

  isolated_world_ = std::make_unique<JavaScriptContentWorld>(
      browser_state_, WKContentWorld.defaultClientWorld);
  AddSharedCommonFeatures(isolated_world_.get());

  for (JavaScriptFeature* feature : features) {
    if (isolated_world_ &&
        feature->GetSupportedContentWorld() !=
            JavaScriptFeature::ContentWorld::kPageContentWorld) {
      isolated_world_->AddFeature(feature);
    } else {
      DCHECK_NE(feature->GetSupportedContentWorld(),
                JavaScriptFeature::ContentWorld::kIsolatedWorldOnly);
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
