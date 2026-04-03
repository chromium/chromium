// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/test/ios_chrome_test_with_web_state.h"
#import "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/test/js_test_util.h"

namespace actor {

// This base class uses the real ChromeWebClient so that the required JavaScript
// files are loaded. If the FakeWebClient is used, all calls to the underlying
// JavaScript files must be mocked with MockJsFunction.
ActorToolJavaScriptFeatureTestBase::ActorToolJavaScriptFeatureTestBase()
    : IOSChromeTestWithWebState(WebClientMode::kChromeWebClient) {
  scoped_feature_list_.InitWithFeatures(
      {web::features::kAssertOnJavaScriptErrors, kActorTools}, {});
}

void ActorToolJavaScriptFeatureTestBase::SetUp() {
  IOSChromeTestWithWebState::SetUp();
  // We load a real HTML page so that we can get and use the main WebFrame in
  // these tests. Using a fake WebFrame would require us to inject and
  // redeclare the dependent JavaScriptFeatures in each test suite.
  NSString* html = base::SysUTF8ToNSString(R"(
          <html>
            <body>
              This HTML is only loaded so that we can get a real WebFrame.
            </body>
          </html>
        )");
  LoadHtml(html);
}

void ActorToolJavaScriptFeatureTestBase::MockJsFunction(
    web::JavaScriptFeature* feature,
    const std::string& api_name,
    const std::string& function_name,
    const std::string& mock_return_value) {
  web::test::ExecuteJavaScriptForFeature(
      web_state(),
      base::SysUTF8ToNSString(base::StringPrintf(
          R"(
        __gCrWeb.getRegisteredApi('%s')
                .addFunction('%s', function() {
                  return %s;
                });
        true;
      )",
          api_name.c_str(), function_name.c_str(), mock_return_value.c_str())),
      feature);
}

base::WeakPtr<web::WebFrame> ActorToolJavaScriptFeatureTestBase::GetMainFrame(
    web::JavaScriptFeature* feature) {
  web::WebFrame* frame = WaitForMainFrame(feature);
  CHECK(frame);
  return frame->AsWeakPtr();
}

}  // namespace actor
