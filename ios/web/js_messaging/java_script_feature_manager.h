// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_

#include <memory>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#import "ios/web/js_messaging/java_script_content_world.h"

//@class WKUserContentController;

namespace web {

class BrowserState;
class JavaScriptFeature;

// Configures JavaScriptFeatures for `browser_state`. The features will be
// added to either `page_content_world_` or `isolated_world_`  based on
// JavaScriptFeature::GetSupportedContentWorld() and the operating system of the
// user's device (which determines if isolated worlds are supported).
class JavaScriptFeatureManager : public base::SupportsUserData::Data {
 public:
  ~JavaScriptFeatureManager() override;

  JavaScriptFeatureManager(const JavaScriptFeatureManager&) = delete;
  JavaScriptFeatureManager& operator=(const JavaScriptFeatureManager&) = delete;

  // Returns the JavaScriptFeatureManager associated with `browser_state`.
  // If a JavaScriptFeatureManager does not already exist, one will be created
  // and associated with `browser_state`. `browser_state` must not be null.
  static JavaScriptFeatureManager* FromBrowserState(
      BrowserState* browser_state);

  // Returns the JavaScriptContentWorld for the page content world associated
  // with `browser_state`. If a JavaScriptFeatureManager does not already exist,
  // one will be created and associated with `browser_state`. `browser_state`
  // must not be null.
  static JavaScriptContentWorld* GetPageContentWorldForBrowserState(
      BrowserState* browser_state);

  // Configures `features` on `user_content_controller_` by adding user scripts
  // and script message handlers.
  // NOTE: `page_content_world_` and `isolated_world_` will be recreated.
  void ConfigureFeatures(std::vector<JavaScriptFeature*> features);

  // Returns the content world associated with `feature` or null if the feature
  // has not be added to the associated `browser_state_`.
  JavaScriptContentWorld* GetContentWorldForFeature(JavaScriptFeature* feature);

  // Returns a list of all the content worlds used by features.
  std::vector<JavaScriptContentWorld*> GetAllContentWorlds();

  // Returns a list of all the content worlds enum values used by features.
  std::vector<ContentWorld> GetAllContentWorldEnums();

 private:
  JavaScriptFeatureManager(BrowserState* browser_state);

  raw_ptr<BrowserState> browser_state_;

  // The content world shared with the page content JavaScript.
  std::unique_ptr<JavaScriptContentWorld> page_content_world_;
  // A content world isolated from the page content JavaScript for application
  // JavaScript execution.
  std::unique_ptr<JavaScriptContentWorld> isolated_world_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_
