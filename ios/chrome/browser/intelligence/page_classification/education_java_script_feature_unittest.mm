// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_classification/education_java_script_feature.h"

#import "base/test/test_future.h"
#import "base/values.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class EducationJavaScriptFeatureTest : public PlatformTest {
 public:
  EducationJavaScriptFeatureTest() = default;

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    web::test::OverrideJavaScriptFeatures(
        &fake_browser_state_, {EducationJavaScriptFeature::GetInstance()});
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeBrowserState fake_browser_state_;
};

// Tests that ExtractDOMFeatures correctly parses valid JSON dictionary output.
TEST_F(EducationJavaScriptFeatureTest, TestExtractDOMFeaturesSuccess) {
  auto fake_frame = web::FakeWebFrame::CreateMainWebFrame();
  fake_frame->set_browser_state(&fake_browser_state_);

  base::DictValue response_dict;
  response_dict.Set("word_count", 650);
  response_dict.Set("heading_count", 4);
  base::Value response_value(std::move(response_dict));

  fake_frame->AddJsResultForFunctionCall(
      &response_value, "education_page_detector.extractDOMFeatures");

  base::test::TestFuture<std::optional<EducationDOMFeatures>> future;
  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      fake_frame.get(), base::Milliseconds(500), future.GetCallback());

  std::optional<EducationDOMFeatures> result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(650, result->word_count);
  EXPECT_EQ(4, result->heading_count);
}

// Tests that ExtractDOMFeatures returns std::nullopt when frame is null.
TEST_F(EducationJavaScriptFeatureTest, TestExtractDOMFeaturesNullFrame) {
  base::test::TestFuture<std::optional<EducationDOMFeatures>> future;
  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      nullptr, base::Milliseconds(500), future.GetCallback());

  std::optional<EducationDOMFeatures> result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// Tests that ExtractDOMFeatures returns std::nullopt on malformed response.
TEST_F(EducationJavaScriptFeatureTest,
       TestExtractDOMFeaturesMalformedResponse) {
  auto fake_frame = web::FakeWebFrame::CreateMainWebFrame();
  fake_frame->set_browser_state(&fake_browser_state_);

  base::Value response_value("invalid_string_response");

  fake_frame->AddJsResultForFunctionCall(
      &response_value, "education_page_detector.extractDOMFeatures");

  base::test::TestFuture<std::optional<EducationDOMFeatures>> future;
  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      fake_frame.get(), base::Milliseconds(500), future.GetCallback());

  std::optional<EducationDOMFeatures> result = future.Take();
  EXPECT_FALSE(result.has_value());
}

// Tests that ExtractDOMFeatures returns std::nullopt on JS timeout or failure.
TEST_F(EducationJavaScriptFeatureTest, TestExtractDOMFeaturesTimeout) {
  auto fake_frame = web::FakeWebFrame::CreateMainWebFrame();
  fake_frame->set_browser_state(&fake_browser_state_);
  fake_frame->set_force_timeout(true);

  base::test::TestFuture<std::optional<EducationDOMFeatures>> future;
  EducationJavaScriptFeature::GetInstance()->ExtractDOMFeatures(
      fake_frame.get(), base::Milliseconds(50), future.GetCallback());

  std::optional<EducationDOMFeatures> result = future.Take();
  EXPECT_FALSE(result.has_value());
}
