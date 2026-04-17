// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_task.h"

#import "base/token.h"
#import "base/types/strong_alias.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ActorTaskTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    task_ = std::make_unique<ActorTask>(ActorTaskId(1), "Test Task");
  }

  void TearDown() override {
    task_.reset();
    PlatformTest::TearDown();
  }

  void AddControlledWebState(base::WeakPtr<web::WebState> web_state) {
    task_->controlled_web_states_.push_back(web_state);
  }

  const std::vector<base::WeakPtr<web::WebState>>& GetControlledWebStates()
      const {
    return task_->controlled_web_states_;
  }

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

}  // namespace actor
