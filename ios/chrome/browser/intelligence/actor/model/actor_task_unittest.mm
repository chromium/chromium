// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import "base/strings/string_number_conversions.h"
#import "base/token.h"
#import "base/types/strong_alias.h"
#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class MockTool : public ActorTool {
 public:
  MockTool(base::WeakPtr<web::WebState> web_state) : web_state_(web_state) {}
  ~MockTool() override = default;

  void Execute(ToolExecutionCallback callback) override {
    std::move(callback).Run(ToolExecutionResult::Ok());
  }

  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return web_state_;
  }

  optimization_guide::proto::Action::ActionCase GetActionCase() const override {
    return optimization_guide::proto::Action::ACTION_NOT_SET;
  }

 private:
  base::WeakPtr<web::WebState> web_state_;
};

class ActorTaskTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    journal_ = std::make_unique<AggregatedJournal>();
    task_ = std::make_unique<ActorTask>(ActorTaskId(1), "Test Task",
                                        /*allow_incognito_web_states=*/false,
                                        journal_.get());
  }

  void TearDown() override {
    task_.reset();
    journal_.reset();
    PlatformTest::TearDown();
  }

  void AddControlledWebState(base::WeakPtr<web::WebState> web_state) {
    task_->controlled_web_states_.push_back(web_state);
  }

  void AddControlledWebStates(
      const std::vector<std::unique_ptr<ActorTool>>& actions) {
    task_->AddControlledWebStates(actions);
  }

  const std::vector<base::WeakPtr<web::WebState>>& GetControlledWebStates()
      const {
    return task_->controlled_web_states_;
  }

  void SetTaskState(ActorTaskState state) { task_->SetState(state); }

  std::unique_ptr<AggregatedJournal> journal_;
  std::unique_ptr<ActorTask> task_;
};

// Tests that the task correctly identifies if it is controlling a given
// WebState. It also verifies that it handles destroyed WebStates and null
// pointers gracefully.
TEST_F(ActorTaskTest, IsControllingWebState) {
  std::unique_ptr<web::FakeWebState> web_state1 =
      std::make_unique<web::FakeWebState>();

  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();

  // web_state1 is added, simulate it being controlled.
  AddControlledWebState(web_state1->GetWeakPtr());

  EXPECT_TRUE(task_->IsControllingWebState(web_state1.get()));
  EXPECT_FALSE(task_->IsControllingWebState(web_state2.get()));

  // Test that when the web state is destroyed, it returns false instead of
  // crashing.
  web_state1.reset();

  // Create a third fake webstate to pass as parameter, just to ensure it safely
  // walks past the now-destroyed weak pointer.
  std::unique_ptr<web::FakeWebState> web_state3 =
      std::make_unique<web::FakeWebState>();

  EXPECT_FALSE(task_->IsControllingWebState(web_state3.get()));

  // Test that passing nullptr returns false gracefully.
  EXPECT_FALSE(task_->IsControllingWebState(nullptr));
}

// Tests that the getter for controlled WebStates returns the correct list of
// WebStates.
TEST_F(ActorTaskTest, ControlledWebStatesGetter) {
  std::unique_ptr<web::FakeWebState> web_state =
      std::make_unique<web::FakeWebState>();
  AddControlledWebState(web_state->GetWeakPtr());

  const auto& controlled_states = GetControlledWebStates();
  EXPECT_EQ(1u, controlled_states.size());
  EXPECT_EQ(web_state.get(), controlled_states[0].get());
}

// Tests that SetState updates the state and logs to the journal.
TEST_F(ActorTaskTest, SetState) {
  SetTaskState(ActorTaskState::kActing);

  EXPECT_EQ(ActorTaskState::kActing, task_->GetState());

  std::vector<JournalEntry> logs = journal_->GetLogs();
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ("ActorTask::SetState", logs[0].event);

  ASSERT_EQ(2u, logs[0].details.size());
  EXPECT_EQ("current_state", logs[0].details[0].key);
  EXPECT_EQ("Init", logs[0].details[0].value);
  EXPECT_EQ("new_state", logs[0].details[1].key);
  EXPECT_EQ("Acting", logs[0].details[1].value);
}

// Tests that AddControlledWebStates correctly adds targeted tabs and ignores
// duplicates.
TEST_F(ActorTaskTest, AddControlledWebStates) {
  std::unique_ptr<web::FakeWebState> web_state1 =
      std::make_unique<web::FakeWebState>();
  std::unique_ptr<web::FakeWebState> web_state2 =
      std::make_unique<web::FakeWebState>();

  std::vector<std::unique_ptr<ActorTool>> actions;
  actions.push_back(std::make_unique<MockTool>(web_state1->GetWeakPtr()));
  actions.push_back(std::make_unique<MockTool>(web_state2->GetWeakPtr()));
  actions.push_back(
      std::make_unique<MockTool>(web_state1->GetWeakPtr()));  // Duplicate

  AddControlledWebStates(actions);

  const auto& controlled_states = GetControlledWebStates();
  EXPECT_EQ(2u, controlled_states.size());
  EXPECT_EQ(web_state1.get(), controlled_states[0].get());
  EXPECT_EQ(web_state2.get(), controlled_states[1].get());
}

}  // namespace actor
