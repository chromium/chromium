// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_java_script_feature.h"

#import "base/gtest_prod_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/timer/elapsed_timer.h"
#import "base/values.h"
#import "components/shared_highlighting/core/common/shared_highlighting_features.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/link_to_text/model/link_generation_outcome.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
base::Value GetSuccessValue() {
  auto dict =
      base::Value::Dict()
          .Set("status", static_cast<int>(LinkGenerationOutcome::kSuccess))
          .Set("fragment", base::Value::Dict().Set("textStart", "text"))
          .Set("selectedText", "text")
          .Set("selectionRect", base::Value::Dict()
                                    .Set("x", 0.0)
                                    .Set("y", 0.0)
                                    .Set("width", 50.0)
                                    .Set("height", 50.0));

  return base::Value(std::move(dict));
}

base::Value GetNoSelectionValue() {
  auto dict = base::Value::Dict().Set(
      "status", static_cast<int>(LinkGenerationOutcome::kInvalidSelection));
  return base::Value(std::move(dict));
}

base::Value GetFailureValue() {
  auto dict = base::Value::Dict().Set(
      "status", static_cast<int>(LinkGenerationOutcome::kExecutionFailed));
  return base::Value(std::move(dict));
}

// Fake that disarms the JS calls but leaves all other logic intact.
class DisarmedFeature : public LinkToTextJavaScriptFeature {
 public:
  void SetResponse(web::WebFrame* frame, base::Value* value) {
    response_map_[frame] = value;
  }

  bool WasJsInvokedInFrame(web::WebFrame* frame) {
    return invoked_.count(frame);
  }

 protected:
  void RunGenerationJS(
      web::WebFrame* frame,
      base::OnceCallback<void(const base::Value*)> callback) override {
    DCHECK(response_map_[frame]);
    invoked_.insert(frame);
    std::move(callback).Run(response_map_[frame]);
  }

 private:
  std::set<web::WebFrame*> invoked_;
  std::map<web::WebFrame*, base::Value*> response_map_;
};
}  // namespace

class LinkToTextJavaScriptFeatureTest : public PlatformTest {
 public:
  LinkToTextJavaScriptFeatureTest() {}

  void SetUp() override {
    web_state_.SetTitle(u"Main Frame Title");
    web_state_.SetWebFramesManager(
        std::make_unique<web::FakeWebFramesManager>());

    UIView* fake_view = [[UIView alloc] init];
    web_state_.SetView(fake_view);

    // Fake Navigation End for UKM setup.
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.OnNavigationStarted(&context);
    web_state_.OnNavigationFinished(&context);
  }

  void AddMainFrame(const GURL& url, base::Value* response_value) {
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
    feature_.SetResponse(main_frame.get(), response_value);
    manager()->AddWebFrame(std::move(main_frame));
    web_state_.SetCurrentURL(url);
  }

  web::FakeWebFramesManager* manager() {
    return static_cast<web::FakeWebFramesManager*>(
        web_state_.GetPageWorldWebFramesManager());
  }

  void InvokeGenerationAndExpectSuccess() {
    feature_.GetLinkToText(
        &web_state_, base::BindOnce([](LinkToTextResponse* response) {
          EXPECT_TRUE(response.payload != nil);
          EXPECT_EQ(base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
                    response.latency);
        }));
  }

  void InvokeGenerationAndExpectError(
      shared_highlighting::LinkGenerationError expected) {
    feature_.GetLinkToText(
        &web_state_,
        base::BindOnce(
            [](shared_highlighting::LinkGenerationError expected_error,
               LinkToTextResponse* response) {
              EXPECT_TRUE(response.error);
              EXPECT_EQ(expected_error, *response.error);
              EXPECT_EQ(base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
                        response.latency);
            },
            expected));
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  base::ScopedMockElapsedTimersForTest timer_;
  DisarmedFeature feature_;
  web::FakeWebState web_state_;
};

TEST_F(LinkToTextJavaScriptFeatureTest, ShouldAttemptIframeGeneration) {
  {
    // Test that ifram generation is false.
    EXPECT_FALSE(LinkToTextJavaScriptFeature::ShouldAttemptIframeGeneration(
        shared_highlighting::LinkGenerationError::kIncorrectSelector,
        GURL("https://www.google.com/amp/")));
  }
}

TEST_F(LinkToTextJavaScriptFeatureTest, GenerateInMainFrame) {
  base::Value success = GetSuccessValue();
  AddMainFrame(GURL("https://www.example.com"), &success);
  InvokeGenerationAndExpectSuccess();
}

TEST_F(LinkToTextJavaScriptFeatureTest, FailInMainFrame) {
  base::Value failure = GetFailureValue();
  AddMainFrame(GURL("https://www.example.com"), &failure);
  InvokeGenerationAndExpectError(
      shared_highlighting::LinkGenerationError::kUnknown);
}

TEST_F(LinkToTextJavaScriptFeatureTest, SuccessInMainFramePreemptsIframes) {
  base::Value success = GetSuccessValue();
  AddMainFrame(GURL("https://www.google.com/amp/"), &success);

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(
      GURL("https://www.ampproject.org"));
  auto* child_frame_raw = child_frame.get();
  base::Value failure = GetFailureValue();
  feature_.SetResponse(child_frame.get(), &failure);
  manager()->AddWebFrame(std::move(child_frame));

  InvokeGenerationAndExpectSuccess();

  EXPECT_FALSE(feature_.WasJsInvokedInFrame(child_frame_raw));
}

TEST_F(LinkToTextJavaScriptFeatureTest, FailInMainFramePreemptsIframes) {
  base::Value failure = GetFailureValue();
  AddMainFrame(GURL("https://www.google.com/amp/"), &failure);

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(
      GURL("https://www.ampproject.org"));
  auto* child_frame_raw = child_frame.get();
  feature_.SetResponse(child_frame.get(), &failure);
  manager()->AddWebFrame(std::move(child_frame));

  InvokeGenerationAndExpectError(
      shared_highlighting::LinkGenerationError::kUnknown);

  EXPECT_FALSE(feature_.WasJsInvokedInFrame(child_frame_raw));
}

TEST_F(LinkToTextJavaScriptFeatureTest, NoIframeGenerationOnUnknownDomain) {
  base::Value noselect = GetNoSelectionValue();
  AddMainFrame(GURL("https://www.example.com"), &noselect);

  auto child_frame = web::FakeWebFrame::CreateChildWebFrame(
      GURL("https://www.ampproject.org"));
  auto* child_frame_raw = child_frame.get();
  base::Value failure = GetFailureValue();
  feature_.SetResponse(child_frame.get(), &failure);
  manager()->AddWebFrame(std::move(child_frame));

  InvokeGenerationAndExpectError(
      shared_highlighting::LinkGenerationError::kIncorrectSelector);

  EXPECT_FALSE(feature_.WasJsInvokedInFrame(child_frame_raw));
}
