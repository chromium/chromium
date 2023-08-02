// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#import "base/ios/ios_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"

namespace {

// Key used to associate a JavaScriptFeatureManager instances with a
// BrowserState.
const char kWebJavaScriptFeatureManagerKeyName[] =
    "web_java_script_feature_manager";

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

  isolated_world_ = std::make_unique<JavaScriptContentWorld>(
      browser_state_, WKContentWorld.defaultClientWorld);

  for (JavaScriptFeature* feature : features) {
    switch (feature->GetSupportedContentWorld()) {
      case ContentWorld::kAllContentWorlds:
        isolated_world_->AddFeature(feature);
        page_content_world_->AddFeature(feature);
        break;
      case ContentWorld::kIsolatedWorld:
        isolated_world_->AddFeature(feature);
        break;
      case ContentWorld::kPageContentWorld:
        page_content_world_->AddFeature(feature);
        break;
    }
  }
}

JavaScriptContentWorld* JavaScriptFeatureManager::GetContentWorldForFeature(
    JavaScriptFeature* feature) {
  if (isolated_world_->HasFeature(feature)) {
    return isolated_world_.get();
  }
  if (page_content_world_->HasFeature(feature)) {
    return page_content_world_.get();
  }
  return nullptr;
}

std::vector<JavaScriptContentWorld*>
JavaScriptFeatureManager::GetAllContentWorlds() {
  return {isolated_world_.get(), page_content_world_.get()};
}

std::vector<ContentWorld> JavaScriptFeatureManager::GetAllContentWorldEnums() {
  // Return these from WKContentWorld directly instead of from
  // JavaScriptContentWorld instances because JavaScriptContentWorld are not
  // created until after `ConfigureFeatures`.
  return {ContentWorld::kPageContentWorld, ContentWorld::kIsolatedWorld};
}

}  // namespace web
