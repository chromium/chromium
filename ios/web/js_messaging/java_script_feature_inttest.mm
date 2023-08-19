// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

static NSString* kPageHTML =
    @"<html><body>"
     "  <div id=\"div\">contents1</div><div id=\"div2\">contents2</div>"
     "</body></html>";

namespace web {

// typedef WebTestWithWebState JavaScriptFeatureTest;
// Sets up a FakeJavaScriptFeature in the page content world.
class JavaScriptFeaturePageContentWorldTest : public WebTestWithWebState {
 protected:
  JavaScriptFeaturePageContentWorldTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ContentWorld::kPageContentWorld) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();

    static_cast<web::FakeWebClient*>(WebTestWithWebState::GetWebClient())
        ->SetJavaScriptFeatures({feature()});
  }

  WebFrame* GetMainFrame() {
    return feature()->GetWebFramesManager(web_state())->GetMainWebFrame();
  }

  FakeJavaScriptFeature* feature() { return &feature_; }

 private:
  FakeJavaScriptFeature feature_;
};

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in the page content world.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       JavaScriptFeatureInjectJavaScript) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in the page content world.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       JavaScriptFeatureExecuteJavaScript) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature()->ReplaceDivContents(GetMainFrame());

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in the page content world.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       MessageHandlerInPageContentWorld) {
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->body());
  const std::string* reply =
      feature()->last_received_message()->body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a page which overrides the window.webkit object does not break the
// JavaScriptFeature JS->native messaging system when the feature script is
// using `sendWebKitMessage` from ios/web/public/js_messaging/resources/utils.ts
TEST_F(JavaScriptFeaturePageContentWorldTest,
       MessagingWithOverriddenWebkitObject) {
  LoadHtml(kPageHTML);
  ExecuteJavaScript(@"webkit = undefined;");

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->body());
  const std::string* reply =
      feature()->last_received_message()->body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a page which overrides the window.webkit object does not break the
// JavaScriptFeature JS->native messaging system when the feature script is
// using `__gCrWeb.common.sendWebKitMessage`
TEST_F(JavaScriptFeaturePageContentWorldTest,
       MessagingWithOverriddenWebkitObjectCommonJS) {
  LoadHtml(kPageHTML);
  ExecuteJavaScript(@"webkit = undefined;");

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessageCommonJS(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->body());
  const std::string* reply =
      feature()->last_received_message()->body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a JavaScriptFeature with
// ReinjectionBehavior::kReinjectOnDocumentRecreation re-injects JavaScript in
// the page content world.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       ReinjectionBehaviorPageContentWorld) {
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  __block bool count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             ASSERT_EQ(0ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             ASSERT_EQ(1ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             EXPECT_EQ(2ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));
}

// Sets up a FakeJavaScriptFeature in an isolated world.
class JavaScriptFeatureAnyContentWorldTest : public WebTestWithWebState {
 protected:
  JavaScriptFeatureAnyContentWorldTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ContentWorld::kIsolatedWorld) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();

    static_cast<web::FakeWebClient*>(WebTestWithWebState::GetWebClient())
        ->SetJavaScriptFeatures({feature()});
  }

  WebFrame* GetMainFrame() {
    return feature()->GetWebFramesManager(web_state())->GetMainWebFrame();
  }

  FakeJavaScriptFeature* feature() { return &feature_; }

 private:
  FakeJavaScriptFeature feature_;
};

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in an isolated world.
TEST_F(JavaScriptFeatureAnyContentWorldTest,
       JavaScriptFeatureInjectJavaScriptIsolatedWorld) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in an isolated world.
TEST_F(JavaScriptFeatureAnyContentWorldTest,
       JavaScriptFeatureExecuteJavaScriptInIsolatedWorld) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature()->ReplaceDivContents(GetMainFrame());

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in an isolated world.
TEST_F(JavaScriptFeatureAnyContentWorldTest, MessageHandlerInIsolatedWorld) {
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->body());
  const std::string* reply =
      feature()->last_received_message()->body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a JavaScriptFeature with
// ReinjectionBehavior::kReinjectOnDocumentRecreation re-injects JavaScript in
// an isolated world.
TEST_F(JavaScriptFeatureAnyContentWorldTest, ReinjectionBehaviorIsolatedWorld) {
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  __block bool count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             ASSERT_EQ(0ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             ASSERT_EQ(1ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature()->GetErrorCount(GetMainFrame(),
                           base::BindOnce(^void(const base::Value* count) {
                             ASSERT_TRUE(count);
                             ASSERT_TRUE(count->is_double());
                             EXPECT_EQ(2ul, count->GetDouble());
                             count_received = true;
                           }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));
}

// Sets up a FakeJavaScriptFeature in an isolated world using
// `ContentWorld::kIsolatedWorld`.
class JavaScriptFeatureIsolatedWorldTest : public WebTestWithWebState {
 protected:
  JavaScriptFeatureIsolatedWorldTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ContentWorld::kIsolatedWorld) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();

    static_cast<web::FakeWebClient*>(WebTestWithWebState::GetWebClient())
        ->SetJavaScriptFeatures({feature()});
  }

  FakeJavaScriptFeature* feature() { return &feature_; }

 private:
  FakeJavaScriptFeature feature_;
};

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in an isolated world only.
TEST_F(JavaScriptFeatureIsolatedWorldTest,
       JavaScriptFeatureExecuteJavaScriptInIsolatedWorldOnly) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  WebFrame* frame =
      feature()->GetWebFramesManager(web_state())->GetMainWebFrame();
  feature()->ReplaceDivContents(frame);

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

}  // namespace web
