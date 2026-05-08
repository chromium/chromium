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
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

// A mock tool for testing.
class MockTool : public ActorTool {
 public:
  MockTool(bool success,
           optimization_guide::proto::Action::ActionCase action_case =
               optimization_guide::proto::Action::ACTION_NOT_SET,
           base::WeakPtr<web::WebState> web_state = nullptr)
      : success_(success), action_case_(action_case), web_state_(web_state) {}
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
    return web_state_;
  }

  optimization_guide::proto::Action::ActionCase GetActionCase() const override {
    return action_case_;
  }

 private:
  bool success_;
  optimization_guide::proto::Action::ActionCase action_case_;
  base::WeakPtr<web::WebState> web_state_;
};

struct DelegateCall {
  optimization_guide::proto::Action::ActionCase tool_case;
  web::WebStateID web_state_id;
};

// A mock ActorEngine::ExecutionUpdatesDelegate for testing.
class MockActorEngineExecutionUpdatesDelegate
    : public ActorEngine::ExecutionUpdatesDelegate {
 public:
  MockActorEngineExecutionUpdatesDelegate() = default;
  ~MockActorEngineExecutionUpdatesDelegate() override = default;

  void OnWillExecuteTool(
      optimization_guide::proto::Action::ActionCase tool_case,
      web::WebStateID web_state_id) override {
    calls_.push_back({tool_case, web_state_id});
    on_will_execute_called_ = true;
  }

  std::vector<DelegateCall> calls_;
  bool on_will_execute_called_ = false;
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
        engine_(ActorTaskId(), journal_.get(), &mock_delegate_) {}

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
  MockActorEngineExecutionUpdatesDelegate mock_delegate_;
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
  EXPECT_EQ(results.size(), 1U);
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
  EXPECT_EQ(results.size(), 1U);
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
  EXPECT_EQ(results.size(), 2U);
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
  EXPECT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].tool_result.IsOk());
  EXPECT_TRUE(results[1].tool_result.IsOk());
}

// Tests the helper method that maps the 1-based `next_action_index_` to the
// 0-based current action index.
TEST_F(ActorEngineTest, InProgressActionIndex) {
  SetNextActionIndex(1);
  EXPECT_EQ(InProgressActionIndex(), 0U);

  SetNextActionIndex(2);
  EXPECT_EQ(InProgressActionIndex(), 1U);
}

// Tests the specific codepath in `CompleteActions` where a failure result
// overwrites a previously recorded success for the same action (e.g., if a
// post-invoke step fails).
TEST_F(ActorEngineTest, CompleteActionsOverwrite) {
  PushActionResult(ActionResult(ToolExecutionResult::Ok()));
  SetNextActionIndex(1);

  CompleteActions(ActionResult(
      ToolExecutionResult(mojom::ActionResultCode::kArgumentsInvalid)));

  EXPECT_EQ(GetActionResults().size(), 1U);
  EXPECT_FALSE(GetActionResults()[0].tool_result.IsOk());
  EXPECT_EQ(GetState(), ActorEngine::State::kFailed);
}

// Tests that the delegate's OnWillExecuteTool callback is fired
// just before tool execution with correct, unique parameters for every tool in
// the sequence.
TEST_F(ActorEngineTest, OnWillExecuteToolCalled) {
  web::FakeWebState web_state1;
  web::FakeWebState web_state2;

  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(
      true, optimization_guide::proto::Action::kClick,
      web_state1.GetWeakPtr()));
  actions.push_back(std::make_unique<MockTool>(
      true, optimization_guide::proto::Action::kNavigate,
      web_state2.GetWeakPtr()));

  base::RunLoop run_loop;
  engine_.Act(
      std::move(actions),
      base::BindOnce([](base::RunLoop* loop,
                        std::vector<ActionResult> res) { loop->Quit(); },
                     &run_loop));

  run_loop.Run();

  EXPECT_TRUE(mock_delegate_.on_will_execute_called_);
  ASSERT_EQ(mock_delegate_.calls_.size(), 2U);

  EXPECT_EQ(mock_delegate_.calls_[0].tool_case,
            optimization_guide::proto::Action::kClick);
  EXPECT_EQ(mock_delegate_.calls_[0].web_state_id,
            web_state1.GetUniqueIdentifier());

  EXPECT_EQ(mock_delegate_.calls_[1].tool_case,
            optimization_guide::proto::Action::kNavigate);
  EXPECT_EQ(mock_delegate_.calls_[1].web_state_id,
            web_state2.GetUniqueIdentifier());
}

// Tests that executing a sequence containing a null tool completes
// with a failure result code (kToolUnknown) instead of crashing.
TEST_F(ActorEngineTest, ActWithNullTool) {
  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(nullptr);

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
  EXPECT_EQ(results.size(), 1U);
  EXPECT_FALSE(results[0].tool_result.IsOk());
  EXPECT_EQ(results[0].tool_result.code(),
            mojom::ActionResultCode::kToolUnknown);
}

}  // namespace actor
