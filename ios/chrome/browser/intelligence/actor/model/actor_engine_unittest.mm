// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_engine.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

// A mock tool for testing.
class MockTool : public ActorTool {
 public:
  MockTool(bool success) : success_(success) {}
  ~MockTool() override = default;

  void Execute(ToolExecutionCallback callback) override {
    if (success_) {
      std::move(callback).Run(ToolExecutionResult::Ok());
    } else {
      std::move(callback).Run(
          ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid));
    }
  }

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return nullptr;
  }

  optimization_guide::proto::Action::ActionCase GetActionCase() const override {
    return optimization_guide::proto::Action::ACTION_NOT_SET;
  }

 private:
  bool success_;
};
}  // namespace

// Test fixture for ActorEngine.
class ActorEngineTest : public PlatformTest {
 protected:
  ActorEngineTest()
      : journal_(std::make_unique<AggregatedJournal>()),
        task_(ActorTaskId(),
              "Test Task",
              /*allow_incognito_web_states=*/false,
              journal_.get()),
        engine_(ActorTaskId(), journal_.get()) {}

  // Wrapper methods to access private members of ActorEngine for testing.

  void SetNextActionIndex(size_t index) { engine_.next_action_index_ = index; }

  size_t InProgressActionIndex() const {
    return engine_.InProgressActionIndex();
  }

  void PushActionResult(ActionResult result) {
    engine_.action_results_.push_back(std::move(result));
  }

  const std::vector<ActionResult>& GetActionResults() const {
    return engine_.action_results_;
  }

  ActorEngine::State GetState() const { return engine_.state_; }

  void CompleteActions(ActionResult&& result) {
    engine_.CompleteActions(std::move(result));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<AggregatedJournal> journal_;
  ActorTask task_;
  ActorEngine engine_;
};

// Tests that a single action executing successfully completes the engine
// sequence with a success result.
TEST_F(ActorEngineTest, ActSuccess) {
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(true));

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_.Act(std::move(actions),
              base::BindOnce(
                  [](bool* called, std::vector<ActionResult>* res_out,
                     base::RunLoop* loop, std::vector<ActionResult> res) {
                    *called = true;
                    res_out->swap(res);
                    loop->Quit();
                  },
                  &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_TRUE(results[0].tool_result.IsOk());
}

// Tests that a single action failing aborts the engine sequence and returns a
// failure result.
TEST_F(ActorEngineTest, ActFailure) {
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(false));

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_.Act(std::move(actions),
              base::BindOnce(
                  [](bool* called, std::vector<ActionResult>* res_out,
                     base::RunLoop* loop, std::vector<ActionResult> res) {
                    *called = true;
                    res_out->swap(res);
                    loop->Quit();
                  },
                  &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_FALSE(results[0].tool_result.IsOk());
}

// Tests that a sequence where the first action succeeds and the second fails
// returns both results, with the second one indicating failure.
TEST_F(ActorEngineTest, ActSequenceSuccessFailure) {
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(true));
  actions.push_back(std::make_unique<MockTool>(false));

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_.Act(std::move(actions),
              base::BindOnce(
                  [](bool* called, std::vector<ActionResult>* res_out,
                     base::RunLoop* loop, std::vector<ActionResult> res) {
                    *called = true;
                    res_out->swap(res);
                    loop->Quit();
                  },
                  &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 2ul);
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_FALSE(results[1].tool_result.IsOk());
}

// Tests that an empty sequence of actions completes immediately with success
// and empty results.
TEST_F(ActorEngineTest, ActEmptySequence) {
  std::vector<std::unique_ptr<ActorTool>> actions;

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_.Act(std::move(actions),
              base::BindOnce(
                  [](bool* called, std::vector<ActionResult>* res_out,
                     base::RunLoop* loop, std::vector<ActionResult> res) {
                    *called = true;
                    res_out->swap(res);
                    loop->Quit();
                  },
                  &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(results.empty());
}

// Tests that multiple actions all executing successfully return success results
// for all actions.
TEST_F(ActorEngineTest, ActMultipleSuccess) {
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(true));
  actions.push_back(std::make_unique<MockTool>(true));

  base::RunLoop run_loop;
  std::vector<ActionResult> results;
  bool callback_called = false;

  engine_.Act(std::move(actions),
              base::BindOnce(
                  [](bool* called, std::vector<ActionResult>* res_out,
                     base::RunLoop* loop, std::vector<ActionResult> res) {
                    *called = true;
                    res_out->swap(res);
                    loop->Quit();
                  },
                  &callback_called, &results, &run_loop));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(results.size(), 2ul);
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_TRUE(results[1].tool_result.IsOk());
}

// Tests the helper method that maps the 1-based `next_action_index_` to the
// 0-based current action index.
TEST_F(ActorEngineTest, InProgressActionIndex) {
  SetNextActionIndex(1);
  EXPECT_EQ(InProgressActionIndex(), 0ul);

  SetNextActionIndex(2);
  EXPECT_EQ(InProgressActionIndex(), 1ul);
}

// Tests the specific codepath in `CompleteActions` where a failure result
// overwrites a previously recorded success for the same action (e.g., if a
// post-invoke step fails).
TEST_F(ActorEngineTest, CompleteActionsOverwrite) {
  PushActionResult(ActionResult(ToolExecutionResult::Ok()));
  SetNextActionIndex(1);

  CompleteActions(ActionResult(
      ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid)));

  EXPECT_EQ(GetActionResults().size(), 1ul);
  EXPECT_FALSE(GetActionResults()[0].tool_result.IsOk());
  EXPECT_EQ(GetState(), ActorEngine::State::kFailed);
}

}  // namespace actor
