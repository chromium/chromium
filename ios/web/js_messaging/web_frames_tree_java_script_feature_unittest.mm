// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frames_tree_java_script_feature.h"

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace web {

// Test fixture for testing with the production WebFramesTreeJavaScriptFeature.
class WebFramesTreeJavaScriptFeatureTest : public WebTestWithWebState {
 protected:
  WebFramesTreeJavaScriptFeatureTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()) {
    scoped_feature_list_.InitAndEnableFeature(web::features::kWebFrameTree);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the content world does not receive the
// org.chromium.registerChildFrame message, but still receives other
// messages sent via postMessage.
TEST_F(WebFramesTreeJavaScriptFeatureTest,
       RegistrationMessageFilteredFromContentWorld) {
  LoadHtml(@"<html><body><script>"
            "  window.receivedRegistrationMessage = false;"
            "  window.receivedOtherMessage = false;"
            "  window.addEventListener('message', (event) => {"
            "    if (event.data && event.data.type === "
            "'org.chromium.registerChildFrame') {"
            "      window.receivedRegistrationMessage = true;"
            "    }"
            "    if (event.data && event.data.type === "
            "'test.other.message') {"
            "      window.receivedOtherMessage = true;"
            "    }"
            "  }, /*use_capture=*/true);"
            "</script></body></html>");

  ExecuteJavaScript(@"window.postMessage({"
                    @"  type: 'org.chromium.registerChildFrame',"
                    @"  parentChildToken: 'token',"
                    @"  secret: 'secret'"
                    @"}, '*');"
                    @"window.postMessage({"
                    @"  type: 'test.other.message',"
                    @"  data: 'hello'"
                    @"}, '*');");

  // Wait for the asynchronous postMessage events to be processed.
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return [ExecuteJavaScript(@"window.receivedOtherMessage") boolValue];
      }));

  EXPECT_FALSE(
      [ExecuteJavaScript(@"window.receivedRegistrationMessage") boolValue]);
  EXPECT_TRUE([ExecuteJavaScript(@"window.receivedOtherMessage") boolValue]);
}

// Tests that script reinjection on document recreation (e.g. document.open())
// preserves parentChildToken and maintains event listener functionality.
TEST_F(WebFramesTreeJavaScriptFeatureTest, DocumentRecreationPreservesToken) {
  LoadHtml(@"<html><body><iframe></iframe></body></html>");

  // Retrieve the initial parentChildToken in the isolated world.
  __block NSString* initial_token = nil;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        initial_token = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
            web_state(),
            @"window[Object.getOwnPropertySymbols(window).find("
            @"s => s.description === 'parentChildToken')];",
            WebFramesTreeJavaScriptFeature::GetInstance());
        return initial_token.length > 0;
      }));

  // Recreate the document using document.open(), write (with an iframe), and
  // close.
  ExecuteJavaScript(
      @"document.open();"
      @"document.write('<html><body><iframe></iframe></body></html>');"
      @"document.close();");

  // Retrieve the parentChildToken after document recreation.
  __block NSString* token_after_recreation = nil;
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        token_after_recreation =
            web::test::ExecuteJavaScriptForFeatureAndReturnResult(
                web_state(),
                @"window[Object.getOwnPropertySymbols(window).find("
                @"s => s.description === 'parentChildToken')];",
                WebFramesTreeJavaScriptFeature::GetInstance());
        return token_after_recreation.length > 0;
      }));

  EXPECT_TRUE([initial_token isEqualToString:token_after_recreation]);
}

}  // namespace web
