// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/wait_tool.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

class WaitToolTest : public PlatformTest {
 public:
  WaitToolTest() { profile_ = TestProfileIOS::Builder().Build(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the tool is created with the default duration when none is
// specified.
TEST_F(WaitToolTest, Create_DefaultDuration) {
  optimization_guide::proto::WaitAction action;
  base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
      WaitTool::Create(action, profile_.get());

  EXPECT_TRUE(result.has_value());

  std::unique_ptr<WaitTool> tool = std::move(result.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Fast forward by the default duration of 3 seconds.
  task_environment_.FastForwardBy(base::Seconds(3));

  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().IsOk());
}

// Tests that the tool is created with the specified duration.
TEST_F(WaitToolTest, Create_SpecifiedDuration) {
  optimization_guide::proto::WaitAction action;
  action.set_wait_time_ms(5000);
  base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
      WaitTool::Create(action, profile_.get());

  EXPECT_TRUE(result.has_value());

  std::unique_ptr<WaitTool> tool = std::move(result.value());
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  // Fast forward by 4.9 seconds, which should not trigger the callback.
  task_environment_.FastForwardBy(base::Milliseconds(4900));
  EXPECT_FALSE(future.IsReady());

  // Fast forward by the remaining 0.1 seconds.
  task_environment_.FastForwardBy(base::Milliseconds(101));
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Get().IsOk());
}

TEST_F(WaitToolTest, GetActionCase) {
  optimization_guide::proto::WaitAction action;
  base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
      WaitTool::Create(action, profile_.get());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value()->GetActionCase(),
            optimization_guide::proto::Action::kWait);
}

}  // namespace

}  // namespace actor
