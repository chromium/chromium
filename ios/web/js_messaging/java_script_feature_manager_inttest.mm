// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

// Sets up a FakeJavaScriptFeature in the page content world.
class JavaScriptFeatureManagerPageContentWorldIntTest
    : public WebTestWithWebState {
 protected:
  JavaScriptFeatureManagerPageContentWorldIntTest()
      : WebTestWithWebState(std::make_unique<web::FakeWebClient>()),
        feature_(ContentWorld::kPageContentWorld) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();

    static_cast<web::FakeWebClient*>(WebTestWithWebState::GetWebClient())
        ->SetJavaScriptFeatures({feature()});
  }

  FakeJavaScriptFeature* feature() { return &feature_; }

 private:
  FakeJavaScriptFeature feature_;
};

// Tests that a JavaScriptFeature added by JavaScriptFeatureManager to the page
// content world correctly receives script message callbacks.
TEST_F(JavaScriptFeatureManagerPageContentWorldIntTest,
       AddFeatureToPageContentWorld) {
  ASSERT_TRUE(LoadHtml("<html></html>"));

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  WebFrame* frame =
      feature()->GetWebFramesManager(web_state())->GetMainWebFrame();

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(frame, parameters);

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

TEST_F(JavaScriptFeatureManagerPageContentWorldIntTest,
       PageContentWorldIFrameScriptMessageHandler) {
  ASSERT_TRUE(LoadHtml("<html><iframe></iframe></html>"));

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  __block std::set<WebFrame*> web_frames;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    web_frames = web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames();
    return web_frames.size() == 2;
  }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : web_frames) {
    if (!frame->IsMainFrame()) {
      child_frame = frame;
      break;
    }
  }

  ASSERT_TRUE(child_frame);

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(child_frame, parameters);

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

// Sets up a FakeJavaScriptFeature in an isolated world.
class JavaScriptFeatureManagerAnyContentWorldIntTest
    : public WebTestWithWebState {
 protected:
  JavaScriptFeatureManagerAnyContentWorldIntTest()
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

TEST_F(JavaScriptFeatureManagerAnyContentWorldIntTest,
       AddFeatureToIsolatedWorld) {
  ASSERT_TRUE(LoadHtml("<html></html>"));

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  WebFrame* frame =
      feature()->GetWebFramesManager(web_state())->GetMainWebFrame();

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(frame, parameters);

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

TEST_F(JavaScriptFeatureManagerAnyContentWorldIntTest,
       IsolatedWorldIFrameScriptMessageHandler) {
  ASSERT_TRUE(LoadHtml("<html><iframe></iframe></html>"));

  ASSERT_FALSE(feature()->last_received_web_state());
  ASSERT_FALSE(feature()->last_received_message());

  __block std::set<WebFrame*> web_frames;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    web_frames = web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames();
    return web_frames.size() == 2;
  }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : web_frames) {
    if (!frame->IsMainFrame()) {
      child_frame = frame;
      break;
    }
  }

  ASSERT_TRUE(child_frame);

  auto parameters =
      base::Value::List().Append(kFakeJavaScriptFeaturePostMessageReplyValue);
  feature()->ReplyWithPostMessage(child_frame, parameters);

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

}  // namespace web
