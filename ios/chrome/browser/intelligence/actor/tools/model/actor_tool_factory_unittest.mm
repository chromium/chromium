// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_error.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ActorToolFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    factory_ = std::make_unique<ActorToolFactory>();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ActorToolFactory> factory_;
};

TEST_F(ActorToolFactoryTest, GetSupportedCapabilities) {
  std::vector<optimization_guide::proto::Action::ActionCase> capabilities =
      factory_->GetSupportedCapabilities();

  EXPECT_THAT(capabilities, testing::UnorderedElementsAre(
                                optimization_guide::proto::Action::kNavigate,
                                optimization_guide::proto::Action::kClick,
                                optimization_guide::proto::Action::kBack,
                                optimization_guide::proto::Action::kForward));
}

TEST_F(ActorToolFactoryTest, CreateToolUnsupported) {
  optimization_guide::proto::Action action;

  base::expected<std::unique_ptr<ActorTool>, ActorToolError> result =
      factory_->CreateTool(action, profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActorToolErrorCode::kUnsupportedAction, result.error().code);
}

}  // namespace actor
