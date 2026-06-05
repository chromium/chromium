// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

// Test fixture for ActorToolFactory.
class ActorToolFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    factory_ = std::make_unique<ActorToolFactory>(profile_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ActorToolFactory> factory_;
};

// Tests that GetSupportedCapabilities returns the expected list of tools when
// all tools are enabled.
TEST_F(ActorToolFactoryTest, GetSupportedCapabilities) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kActorTools);

  std::vector<optimization_guide::proto::Action::ActionCase> capabilities =
      factory_->GetSupportedCapabilities();

  EXPECT_THAT(capabilities, testing::UnorderedElementsAre(
                                optimization_guide::proto::Action::kNavigate,
                                optimization_guide::proto::Action::kClick,
                                optimization_guide::proto::Action::kBack,
                                optimization_guide::proto::Action::kForward,
                                optimization_guide::proto::Action::kType,
                                optimization_guide::proto::Action::kWait,
                                optimization_guide::proto::Action::kScroll,
                                optimization_guide::proto::Action::kScrollTo,
                                optimization_guide::proto::Action::kSelect));
}

// Tests that GetSupportedCapabilities filters out tools that are disabled via
// feature parameters.
TEST_F(ActorToolFactoryTest, GetSupportedCapabilitiesWithDisabledTools) {
  base::test::ScopedFeatureList feature_list;
  // Disable ClickTool via feature parameters.
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTools, {{"DisabledTools", "ClickTool"}});

  std::vector<optimization_guide::proto::Action::ActionCase> capabilities =
      factory_->GetSupportedCapabilities();

  // Verify that the disabled tool is not included in the supported
  // capabilities.
  EXPECT_THAT(capabilities, testing::Not(testing::Contains(
                                optimization_guide::proto::Action::kClick)));

  // Verify that other tools (which are not disabled) are still included.
  EXPECT_THAT(capabilities,
              testing::Contains(optimization_guide::proto::Action::kNavigate));
}

TEST_F(ActorToolFactoryTest, CreateToolUnsupported) {
  optimization_guide::proto::Action action;

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> result =
      factory_->CreateTool(action);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kUnsupportedAction,
            result.error().internal_code().value());
}

}  // namespace actor
