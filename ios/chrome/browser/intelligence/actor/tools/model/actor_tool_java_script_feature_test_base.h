// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_TEST_BASE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_TEST_BASE_H_

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace actor {

// A base class to share logic across unittests for JavaScriptFeature classes
// in //ios/chrome/browser/intelligence/actor/tools/model/,
class ActorToolJavaScriptFeatureTestBase : public IOSChromeTestWithWebState {
 public:
  ActorToolJavaScriptFeatureTestBase();

 protected:
  void SetUp() override;

  // Overrides the `api_name.function_name` JavaScript function to return
  // `mock_return_value` when called.
  void MockJsFunction(web::JavaScriptFeature* feature,
                      const std::string& api_name,
                      const std::string& function_name,
                      const std::string& mock_return_value);

  // Gets the main frame of the web state as a WeakPtr.
  base::WeakPtr<web::WebFrame> GetMainFrame(web::JavaScriptFeature* feature);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTOR_TOOL_JAVA_SCRIPT_FEATURE_TEST_BASE_H_
