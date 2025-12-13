// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_utils.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_language_model_create_core_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/ai/ai_features.h"

namespace blink {

struct MeetsUserActivationRequirementsTestCase {
  std::string test_name;
  bool feature_is_enabled;
  bool consume_transient_user_activation;
  bool expected_output;
};

class AIUtilsTest : public PageTestBase,
                    public testing::WithParamInterface<
                        MeetsUserActivationRequirementsTestCase> {
 public:
  void SetUp() override { PageTestBase::SetUp(); }

  // Grants user activation.
  void GrantUserActivation(LocalFrame* frame) {
    LocalFrame::NotifyUserActivation(
        frame, mojom::blink::UserActivationNotificationType::kTest);
  }

  void ConsumeTransientUserActivation(LocalFrame* frame) {
    LocalFrame::ConsumeTransientUserActivation(frame);
  }

  LocalDOMWindow* GetWindow() { return GetFrame().DomWindow(); }

 protected:
  const MeetsUserActivationRequirementsTestCase& GetTestCase() {
    return GetParam();
  }
};

TEST(ResolveSamplingParamsOptionTest, NoOptions) {
  auto result = ResolveSamplingParamsOption(nullptr);
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value());
}

TEST(ResolveSamplingParamsOptionTest, NoSamplingParams) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_TRUE(result.has_value());
  ASSERT_FALSE(result.value());
}

TEST(ResolveSamplingParamsOptionTest, ValidOptions) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(1);
  options->setTemperature(0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value()->top_k, 1u);
  ASSERT_EQ(result.value()->temperature, 0.5);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTopK(10);
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_TRUE(result2.has_value());
  ASSERT_EQ(result2.value()->top_k, 10u);
  ASSERT_EQ(result2.value()->temperature, 0.5);

  auto* options3 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options3->setTopK(10);
  options3->setTemperature(0);
  auto result3 = ResolveSamplingParamsOption(options3);
  ASSERT_TRUE(result3.has_value());
  ASSERT_EQ(result3.value()->top_k, 10u);
  ASSERT_EQ(result3.value()->temperature, 0);
}

TEST(ResolveSamplingParamsOptionTest, OnlyOneOfTopKAndTemperatureIsProvided) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(10);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(),
            SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_FALSE(result2.has_value());
  ASSERT_EQ(result2.error(),
            SamplingParamsOptionError::kOnlyOneOfTopKAndTemperatureIsProvided);
}

TEST(ResolveSamplingParamsOptionTest, InvalidTopK) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(0.5);
  options->setTemperature(0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), SamplingParamsOptionError::kInvalidTopK);

  auto* options2 = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options2->setTopK(-1);
  options2->setTemperature(0.5);
  auto result2 = ResolveSamplingParamsOption(options2);
  ASSERT_FALSE(result2.has_value());
  ASSERT_EQ(result2.error(), SamplingParamsOptionError::kInvalidTopK);
}

TEST(ResolveSamplingParamsOptionTest, InvalidTemperature) {
  auto* options = MakeGarbageCollected<LanguageModelCreateCoreOptions>();
  options->setTopK(10);
  options->setTemperature(-0.5);
  auto result = ResolveSamplingParamsOption(options);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error(), SamplingParamsOptionError::kInvalidTemperature);
}

TEST_F(AIUtilsTest,
       MeetsUserActivationRequirements_StickyFeatureEnabled_NoUserActivation) {
  base::test::ScopedFeatureList feature_list(kAIRelaxUserActivationReqs);

  // Ensure no activation beforehand.
  EXPECT_FALSE(GetFrame().HasStickyUserActivation());
  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  EXPECT_FALSE(MeetsUserActivationRequirements(GetWindow()));
  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&GetFrame()))
      << "Transient activation never happened";
  EXPECT_FALSE(GetFrame().HasStickyUserActivation())
      << "Sticky activation never happened";
}

TEST_P(AIUtilsTest, MeetsUserActivationRequirementsTest) {
  const MeetsUserActivationRequirementsTestCase& test_case = GetTestCase();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(kAIRelaxUserActivationReqs,
                                    test_case.feature_is_enabled);

  LocalDOMWindow* window = GetDocument().domWindow();

  GrantUserActivation(&GetFrame());

  if (test_case.consume_transient_user_activation) {
    ConsumeTransientUserActivation(&GetFrame());
  }

  EXPECT_TRUE(GetFrame().HasStickyUserActivation());
  EXPECT_EQ(LocalFrame::HasTransientUserActivation(&GetFrame()),
            !test_case.consume_transient_user_activation);

  EXPECT_EQ(MeetsUserActivationRequirements(window), test_case.expected_output);
  EXPECT_EQ(LocalFrame::HasTransientUserActivation(&GetFrame()),
            test_case.feature_is_enabled &&
                !test_case.consume_transient_user_activation)
      << "Transient activation was consumed if it was consumed prior or if the "
         "flag is disabled";
  EXPECT_TRUE(GetFrame().HasStickyUserActivation())
      << "Sticky activation should remain";
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName prefix */,
    AIUtilsTest,
    testing::Values(
        MeetsUserActivationRequirementsTestCase{
            .test_name = "SucceedsWithFeatureDisabledAndTransientActivationOn",
            .feature_is_enabled = false,
            .consume_transient_user_activation = false,
            .expected_output = true},
        MeetsUserActivationRequirementsTestCase{
            .test_name = "FailsWithFeatureDisabledAndTransientActivationOff",
            .feature_is_enabled = false,
            .consume_transient_user_activation = true,
            .expected_output = false},
        MeetsUserActivationRequirementsTestCase{
            .test_name = "SucceedsWithFeatureEnabledAndTransientActivationOn",
            .feature_is_enabled = true,
            .consume_transient_user_activation = false,
            .expected_output = true},
        MeetsUserActivationRequirementsTestCase{
            .test_name = "SucceedsWithFeatureEnabledAndTransientActivationOff",
            .feature_is_enabled = true,
            .consume_transient_user_activation = true,
            .expected_output = true}),
    [](const testing::TestParamInfo<AIUtilsTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace blink
