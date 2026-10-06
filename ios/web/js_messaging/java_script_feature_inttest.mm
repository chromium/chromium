// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

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
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

static NSString* kPageHTML =
    @"<html><body>"
     "  <div id=\"div\">contents1</div><div id=\"div2\">contents2</div>"
     "</body></html>";

// String to be sent back to the page after a message.
const char kReplyString[] = "reply_string";

namespace web {

namespace {

// Returns the main frame of `feature`'s content world in `web_state` once it
// has been registered, or null if it is not registered within
// `kWaitForJSCompletionTimeout`. Frame registration happens asynchronously
// after the page load completes.
WebFrame* WaitForMainFrame(FakeJavaScriptFeature* feature,
                           WebState* web_state) {
  WebFramesManager* manager = feature->GetWebFramesManager(web_state);
  if (!WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return manager->GetMainWebFrame() != nullptr;
      })) {
    return nullptr;
  }
  return manager->GetMainWebFrame();
}

// Returns true once evaluating `script` in `feature`'s content world returns
// `expected`, or false if it does not within `kWaitForJSCompletionTimeout`.
bool WaitForFeatureJavaScriptResult(WebState* web_state,
                                    NSString* script,
                                    FakeJavaScriptFeature* feature,
                                    id expected) {
  return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return [test::ExecuteJavaScriptForFeatureAndReturnResult(
        web_state, script, feature) isEqual:expected];
  });
}

}  // namespace

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

// Tests that a FeatureScript can be executed on demand and honors its
// reinjection behavior across documents.
TEST_F(JavaScriptFeaturePageContentWorldTest, ExecuteFeatureScriptOnDemand) {
  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.onDemandExecutionCount = "
          "    (window.onDemandExecutionCount || 0) + 1;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame);

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  WebFrame* main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);
  EXPECT_NSEQ(
      @"undefined",
      test::ExecuteJavaScriptForFeatureAndReturnResult(
          web_state(), @"typeof window.onDemandExecutionCount", feature()));

  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.onDemandExecutionCount", feature(), @(1)));

  // The default reinjection behavior prevents another execution in the same
  // window.
  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_NSEQ(@(1),
              test::ExecuteJavaScriptForFeatureAndReturnResult(
                  web_state(), @"window.onDemandExecutionCount", feature()));

  // A navigation creates a new JavaScript context, so the same script can run
  // in the new document.
  const std::string first_frame_id = main_frame->GetFrameId();
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);
  ASSERT_NE(first_frame_id, main_frame->GetFrameId());
  EXPECT_NSEQ(
      @"undefined",
      test::ExecuteJavaScriptForFeatureAndReturnResult(
          web_state(), @"typeof window.onDemandExecutionCount", feature()));
  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.onDemandExecutionCount", feature(), @(1)));
}

// Tests that a FeatureScript with kReinjectOnDocumentRecreation runs again
// when executed on demand a second time in the same window.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       ExecuteReinjectableFeatureScriptOnDemandTwice) {
  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.reinjectableExecutionCount = "
          "    (window.reinjectableExecutionCount || 0) + 1;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame,
          JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kReinjectOnDocumentRecreation);

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  WebFrame* main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);

  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.reinjectableExecutionCount", feature(), @(1)));

  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.reinjectableExecutionCount", feature(), @(2)));
}

// Tests that a main-frame-only FeatureScript is not executed in a child
// frame.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       DoNotExecuteMainFrameFeatureScriptInChildFrame) {
  LoadHtml(@"<html><body><iframe></iframe></body></html>");

  WebFramesManager* manager = feature()->GetWebFramesManager(web_state());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return manager->GetAllWebFrames().size() == 2;
      }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : manager->GetAllWebFrames()) {
    if (!frame->IsMainFrame()) {
      child_frame = frame;
      break;
    }
  }
  ASSERT_TRUE(child_frame);

  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.mainFrameOnlyScriptWasExecuted = true;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame);
  EXPECT_FALSE(feature()->ExecuteScript(child_frame, script));
}

// Tests that an all-frames FeatureScript is executed in a child frame.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       ExecuteAllFramesFeatureScriptInChildFrame) {
  LoadHtml(@"<html><body><iframe></iframe></body></html>");

  WebFramesManager* manager = feature()->GetWebFramesManager(web_state());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^bool {
        return manager->GetAllWebFrames().size() == 2;
      }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : manager->GetAllWebFrames()) {
    if (!frame->IsMainFrame()) {
      child_frame = frame;
      break;
    }
  }
  ASSERT_TRUE(child_frame);

  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.allFramesScriptWasExecuted = true;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames);
  EXPECT_TRUE(feature()->ExecuteScript(child_frame, script));

  __block bool completion_block_called = false;
  __block std::optional<base::Value> result_value;
  child_frame->ExecuteJavaScript(u"window.allFramesScriptWasExecuted",
                                 base::BindOnce(^(const base::Value* value) {
                                   completion_block_called = true;
                                   if (value) {
                                     result_value = value->Clone();
                                   }
                                 }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_block_called;
  }));
  ASSERT_TRUE(result_value.has_value());
  ASSERT_TRUE(result_value->is_bool());
  EXPECT_TRUE(result_value->GetBool());
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in the page content world.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       MessageHandlerInPageContentWorld) {
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in the page content world and can reply to it.
TEST_F(JavaScriptFeaturePageContentWorldTest,
       MessageHandlerWithReplyInPageContentWorld) {
  feature()->SetReplyToMessages(true);
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->SetResponseToNextMessage(kReplyString);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->received_message_count() == 2;
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kReplyString, reply->c_str());
}

// Tests that a JavaScriptFeature correctly calls async JavaScript functions.
TEST_F(JavaScriptFeaturePageContentWorldTest, CallAsyncJavaScriptFunction) {
  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));

  __block bool completion_block_called = false;
  __block std::optional<base::Value> result_value;
  __block NSError* result_error = nil;

  auto completion_block = ^(const base::Value* value, NSError* error) {
    completion_block_called = true;
    if (value) {
      result_value = value->Clone();
    }
    result_error = error;
  };

  EXPECT_TRUE(feature()->CallAsyncSum(GetMainFrame(), /*addend1=*/2,
                                      /*addend2=*/3,
                                      base::BindOnce(completion_block)));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return completion_block_called;
  }));

  ASSERT_FALSE(result_error);
  ASSERT_TRUE(result_value.has_value());
  ASSERT_TRUE(result_value->is_double());
  EXPECT_EQ(result_value->GetDouble(), 5.0);
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
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
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
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in an isolated world and can reply to it.
TEST_F(JavaScriptFeatureAnyContentWorldTest,
       MessageHandlerWithReplyInIsolatedWorld) {
  feature()->SetReplyToMessages(true);
  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->SetResponseToNextMessage(kReplyString);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->received_message_count() == 2;
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kReplyString, reply->c_str());
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

// Tests that a FeatureScript executed on demand by a feature configured in an
// isolated world runs in that world only and is not visible to the page
// content world.
TEST_F(JavaScriptFeatureIsolatedWorldTest,
       ExecuteFeatureScriptInIsolatedWorldOnly) {
  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.isolatedWorldScriptWasExecuted = true;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame);

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));

  WebFrame* main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);
  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));

  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.isolatedWorldScriptWasExecuted", feature(), @YES));
  EXPECT_NSEQ(
      @"undefined",
      ExecuteJavaScript(@"typeof window.isolatedWorldScriptWasExecuted"));
}

// Sets up a private FakeJavaScriptFeature.
class JavaScriptFeaturePrivateTest : public WebTestWithWebState {
 protected:
  JavaScriptFeaturePrivateTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ContentWorld::kPageContentWorld,
                 OriginFilter::kValidTestOriginForTesting) {}

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
// in an authorized domain
TEST_F(JavaScriptFeaturePrivateTest, JavaScriptFeatureInjectJavaScript) {
  LoadHtml(kPageHTML, GURL("https://test.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature executes its injected JavaScript when
// in an authorized domain (case insensitive).
TEST_F(JavaScriptFeaturePrivateTest,
       JavaScriptFeatureInjectJavaScriptCaseInsensitive) {
  LoadHtml(kPageHTML, GURL("https://TEST.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature executes its injected JavaScript when
// in an unauthorized domain
TEST_F(JavaScriptFeaturePrivateTest, JavaScriptFeatureDoNotInjectJavaScript) {
  LoadHtml(kPageHTML, GURL("http://invalid.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  // Shorter timeout on this as it is expected to fail.
  EXPECT_FALSE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText, base::Seconds(1)));
}

// Tests that a malicious page cannot bypass the private origin check of a
// JavaScriptFeature by poisoning Array.prototype.includes and calling
// document.open().
TEST_F(JavaScriptFeaturePrivateTest,
       OriginGateBypassViaPrototypePoisoningAndDocumentOpen) {
  LoadHtml(kPageHTML, GURL("http://invalid.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));

  // Confirm the private script did not run initially.
  EXPECT_FALSE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText, base::Seconds(1)));

  // Poison Array.prototype.includes and force script re-injection via
  // document.open()
  ExecuteJavaScript(@"Array.prototype.includes = function() { return true; };"
                     "document.open();"
                     "document.write('<html><body><div "
                     "id=\"div\">contents1</div></body></html>');"
                     "document.close();");

  // Verify that the private script still was NOT injected/executed after
  // re-injection on the unauthorized origin.
  EXPECT_FALSE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText, base::Seconds(1)));
}

// Tests that a private JavaScriptFeature can call JavaScript when on an
// authorized page.
TEST_F(JavaScriptFeaturePrivateTest, CallFunctionOnAllowedPage) {
  LoadHtml(kPageHTML, GURL("https://test.test"));

  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));

  EXPECT_TRUE(feature()->ReplaceDivContents(GetMainFrame()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
}

// Tests that a private JavaScriptFeature can not call JavaScript when on an
// unauthorized domain.
TEST_F(JavaScriptFeaturePrivateTest, CallFunctionOnUnauthorizedPage) {
  LoadHtml(kPageHTML, GURL("http://invalid.test"));

  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));

  EXPECT_FALSE(feature()->ReplaceDivContents(GetMainFrame()));

  // The JavaScript call should not be executed.
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_FALSE(test::WaitForWebViewContainingText(web_state(), "updated"));
}

// Tests that a private JavaScriptFeature receives post messages from JavaScript
// for registered names in the page content world when on an authorized page.
TEST_F(JavaScriptFeaturePrivateTest, MessageHandler) {
  LoadHtml(kPageHTML, GURL("https://test.test/subpage"));

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  auto parameters =
      base::ListValue().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(GetMainFrame(), parameters);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));

  EXPECT_EQ(web_state(), feature()->last_received_web_state());

  ASSERT_TRUE(feature()->last_received_message()->legacy_body());
  const std::string* reply =
      feature()->last_received_message()->legacy_body()->GetIfString();
  ASSERT_TRUE(reply);
  EXPECT_STREQ(kFakeJavaScriptFeaturePostMessageReplyValue, reply->c_str());
}

// Tests that a private JavaScriptFeature receives direct post messages from
// JavaScript for registered names in the page content world when on an
// authorized page.
TEST_F(JavaScriptFeaturePrivateTest, DirectMessageHandlerOnAllowedPage) {
  LoadHtml(kPageHTML, GURL("https://test.test"));
  // Send message directly without using any JavaScriptFeature
  ExecuteJavaScript(
      @"window.webkit.messageHandlers['FakeHandlerName'].postMessage('test');");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));
}

// Tests that a private JavaScriptFeature receives direct post messages from
// JavaScript for registered names in the page content world when on an
// authorized page (case insensitive).
TEST_F(JavaScriptFeaturePrivateTest,
       DirectMessageHandlerOnCaseInsensitivePage) {
  LoadHtml(kPageHTML, GURL("https://TEST.test"));
  // Send message directly without using any JavaScriptFeature
  ExecuteJavaScript(
      @"window.webkit.messageHandlers['FakeHandlerName'].postMessage('test');");
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature()->last_received_web_state();
  }));
}

// Tests that a private JavaScriptFeature receives direct post messages from
// JavaScript for registered names in the page content world when on an
// filtered page.
TEST_F(JavaScriptFeaturePrivateTest, DirectMessageHandlerOnFilteredPage) {
  LoadHtml(kPageHTML, GURL("https://invalid.test"));
  // Send message directly without using any JavaScriptFeature
  ExecuteJavaScript(
      @"window.webkit.messageHandlers['FakeHandlerName'].postMessage('test');");
  // Shorter timeout on this as it is expected to fail.
  ASSERT_FALSE(WaitUntilConditionOrTimeout(base::Seconds(1), ^bool {
    return feature()->last_received_web_state();
  }));
}

// Tests that a private JavaScriptFeature can execute a FeatureScript on demand
// when on an authorized page.
TEST_F(JavaScriptFeaturePrivateTest, ExecuteFeatureScriptOnAllowedPage) {
  LoadHtml(kPageHTML, GURL("https://test.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  WebFrame* main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);

  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.privateScriptWasExecuted = true;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame,
          JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kInjectOncePerWindow,
          JavaScriptFeature::FeatureScript::PlaceholderReplacementsCallback(),
          OriginFilter::kValidTestOriginForTesting);
  EXPECT_TRUE(feature()->ExecuteScript(main_frame, script));
  EXPECT_TRUE(WaitForFeatureJavaScriptResult(
      web_state(), @"window.privateScriptWasExecuted", feature(), @YES));
}

// Tests that a private JavaScriptFeature does not execute a FeatureScript on
// demand when on an unauthorized page.
TEST_F(JavaScriptFeaturePrivateTest, ExecuteFeatureScriptOnUnauthorizedPage) {
  LoadHtml(kPageHTML, GURL("http://invalid.test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  WebFrame* main_frame = WaitForMainFrame(feature(), web_state());
  ASSERT_TRUE(main_frame);

  const JavaScriptFeature::FeatureScript script =
      JavaScriptFeature::FeatureScript::CreateWithString(
          "window.privateScriptWasExecuted = true;",
          JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame,
          JavaScriptFeature::FeatureScript::ReinjectionBehavior::
              kInjectOncePerWindow,
          JavaScriptFeature::FeatureScript::PlaceholderReplacementsCallback(),
          OriginFilter::kValidTestOriginForTesting);
  EXPECT_FALSE(feature()->ExecuteScript(main_frame, script));
  EXPECT_NSEQ(
      @"undefined",
      test::ExecuteJavaScriptForFeatureAndReturnResult(
          web_state(), @"typeof window.privateScriptWasExecuted", feature()));
}

}  // namespace web
