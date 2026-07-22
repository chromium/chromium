// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/wait_tool.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/web/public/web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

namespace {

class WaitToolTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult>
  CreateToolAndValidate(const optimization_guide::proto::WaitAction& action,
                        web::WebState* web_state) {
    auto tool =
        WaitTool::Create(web_state ? web_state->GetWeakPtr() : nullptr, action);
    CHECK(tool);
    base::test::TestFuture<ToolExecutionResult> validate_future;
    tool->Validate(validate_future.GetCallback());
    if (!validate_future.Get().IsOk()) {
      return base::unexpected(validate_future.Get());
    }
    return tool;
  }
};

// Tests that the tool is created with the default duration when none is
// specified.
TEST_F(WaitToolTest, Create_DefaultDuration) {
  optimization_guide::proto::WaitAction action;
  base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
      CreateToolAndValidate(action, nullptr);

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
      CreateToolAndValidate(action, nullptr);

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

TEST_F(WaitToolTest, GetToolType) {
  // Test non-zero duration (default duration).
  {
    optimization_guide::proto::WaitAction action;
    base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
        CreateToolAndValidate(action, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->GetToolType(), ToolType::kWait);
  }

  // Test non-zero duration (explicitly specified).
  {
    optimization_guide::proto::WaitAction action;
    action.set_wait_time_ms(5000);
    base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
        CreateToolAndValidate(action, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->GetToolType(), ToolType::kWait);
  }

  // Test zero duration.
  {
    optimization_guide::proto::WaitAction action;
    action.set_wait_time_ms(0);
    base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
        CreateToolAndValidate(action, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->GetToolType(), ToolType::kWaitZeroDuration);
  }

  // Test negative duration.
  {
    optimization_guide::proto::WaitAction action;
    action.set_wait_time_ms(-1000);
    base::expected<std::unique_ptr<WaitTool>, ToolExecutionResult> result =
        CreateToolAndValidate(action, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value()->GetToolType(), ToolType::kWaitZeroDuration);
  }
}

}  // namespace

}  // namespace actor
