// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_
#define IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/supports_user_data.h"
#import "ios/web/js_messaging/java_script_content_world.h"

@class WKUserContentController;

namespace web {

class BrowserState;
class JavaScriptFeature;

// Configures JavaScriptFeatures for |browser_state|. The features will be
// added to either |page_content_world_| or |isolated_world_|  based on
// JavaScriptFeature::GetSupportedContentWorld() and the operating system of the
// user's device (which determines if isolated worlds are supported).
class JavaScriptFeatureManager : public base::SupportsUserData::Data {
 public:
  ~JavaScriptFeatureManager() override;

  // Returns the JavaScriptFeatureManager associated with |browser_state.|
  // If a JavaScriptFeatureManager does not already exist, one will be created
  // and associated with |browser_state|. |browser_state| must not be null.
  static JavaScriptFeatureManager* FromBrowserState(
      BrowserState* browser_state);

  // Configures |features| on |user_content_controller_| by adding user scripts
  // and script message handlers.
  // NOTE: |page_content_world_| and |isolated_world_| will be recreated.
  void ConfigureFeatures(std::vector<JavaScriptFeature*> features);

  JavaScriptFeatureManager(const JavaScriptFeatureManager&) = delete;
  JavaScriptFeatureManager& operator=(const JavaScriptFeatureManager&) = delete;

 private:
  JavaScriptFeatureManager(WKUserContentController* user_content_controller);

  WKUserContentController* user_content_controller_ = nullptr;

  // The content world shared with the page content JavaScript.
  std::unique_ptr<JavaScriptContentWorld> page_content_world_;
  // A content world isolated from the page content JavaScript for application
  // JavaScript execution.
  std::unique_ptr<JavaScriptContentWorld> isolated_world_;
};

}  // namespace web

#endif  // IOS_WEB_JS_MESSAGING_JAVA_SCRIPT_FEATURE_MANAGER_H_
