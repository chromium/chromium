// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import <memory>
#import <sstream>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/test/run_until.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using actor::ActorControlState;

class FakeActorTabHelperObserver : public ActorTabHelperObserver {
 public:
  FakeActorTabHelperObserver() = default;
  FakeActorTabHelperObserver(const FakeActorTabHelperObserver&) = delete;
  FakeActorTabHelperObserver& operator=(const FakeActorTabHelperObserver&) =
      delete;
  FakeActorTabHelperObserver(FakeActorTabHelperObserver&&) = delete;
  FakeActorTabHelperObserver& operator=(FakeActorTabHelperObserver&&) = delete;

  // ActorTabHelperObserver:
  void OnControlStateChanged(ActorTabHelper* tab_helper,
                             web::WebState* web_state,
                             ActorControlState previous_control_state,
                             ActorControlState new_control_state) override;

  bool on_control_state_changed_called_ = false;
  raw_ptr<web::WebState> last_web_state_value_ = nullptr;
  ActorControlState last_previous_control_state_value_ =
      ActorControlState::kInactive;
  ActorControlState last_new_control_state_value_ =
      ActorControlState::kInactive;
};

void FakeActorTabHelperObserver::OnControlStateChanged(
    ActorTabHelper* tab_helper,
    web::WebState* web_state,
    ActorControlState previous_control_state,
    ActorControlState new_control_state) {
  on_control_state_changed_called_ = true;
  last_web_state_value_ = web_state;
  last_previous_control_state_value_ = previous_control_state;
  last_new_control_state_value_ = new_control_state;
}

class FakeLegacyActorTabHelperObserver : public ActorTabHelperObserver {
 public:
  // ActorTabHelperObserver:
  void OnActuationStateChanged(ActorTabHelper* tab_helper,
                               web::WebState* web_state,
                               bool actuating) override {
    on_actuation_state_changed_called_ = true;
    last_actuating_value_ = actuating;
  }

  bool on_actuation_state_changed_called_ = false;
  bool last_actuating_value_ = false;
};

// A FakeWebState subclass that records the last value passed to
// `SetKeepRenderProcessAlive`, which the base fake ignores.
class KeepAliveFakeWebState : public web::FakeWebState {
 public:
  void SetKeepRenderProcessAlive(bool keep_alive) override {
    keep_render_process_alive_ = keep_alive;
  }
  bool keep_render_process_alive() const { return keep_render_process_alive_; }

 private:
  bool keep_render_process_alive_ = false;
};

class ActorTabHelperTest : public PlatformTest {
 public:
  ActorTabHelperTest() = default;
  ActorTabHelperTest(const ActorTabHelperTest&) = delete;
  ActorTabHelperTest& operator=(const ActorTabHelperTest&) = delete;
  ActorTabHelperTest(ActorTabHelperTest&&) = delete;
  ActorTabHelperTest& operator=(ActorTabHelperTest&&) = delete;

 protected:
  // PlatformTest:
  void SetUp() override;
  void TearDown() override;

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<KeepAliveFakeWebState> web_state_;
  raw_ptr<ActorTabHelper> tab_helper_ = nullptr;
};

void ActorTabHelperTest::SetUp() {
  PlatformTest::SetUp();
  web_state_ = std::make_unique<KeepAliveFakeWebState>();
  ActorTabHelper::CreateForWebState(web_state_.get());
  tab_helper_ = ActorTabHelper::FromWebState(web_state_.get());
}

void ActorTabHelperTest::TearDown() {
  tab_helper_ = nullptr;
  PlatformTest::TearDown();
}

// Test that the tab helper is created with control state set to kInactive by
// default.
TEST_F(ActorTabHelperTest, DefaultControlState) {
  ASSERT_NE(tab_helper_, nullptr);
  EXPECT_EQ(tab_helper_->GetControlState(), ActorControlState::kInactive);
}

// Test setting and getting the control state.
TEST_F(ActorTabHelperTest, SetAndGetControlState) {
  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_EQ(tab_helper_->GetControlState(),
            ActorControlState::kActorControlled);

  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_EQ(tab_helper_->GetControlState(), ActorControlState::kInactive);
}

// Test that observers are notified when the control state changes.
TEST_F(ActorTabHelperTest, ObserverNotification) {
  FakeActorTabHelperObserver observer;
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(tab_helper_);

  EXPECT_FALSE(observer.on_control_state_changed_called_);

  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_control_state_changed_called_);
  EXPECT_EQ(observer.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer.last_previous_control_state_value_,
            ActorControlState::kInactive);
  EXPECT_EQ(observer.last_new_control_state_value_,
            ActorControlState::kActorControlled);

  observer.on_control_state_changed_called_ = false;
  tab_helper_->SetControlState(
      ActorControlState::kActorControlled);  // No change.
  EXPECT_FALSE(observer.on_control_state_changed_called_);

  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_TRUE(observer.on_control_state_changed_called_);
  EXPECT_EQ(observer.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer.last_previous_control_state_value_,
            ActorControlState::kActorControlled);
  EXPECT_EQ(observer.last_new_control_state_value_,
            ActorControlState::kInactive);
}

// Test that multiple observers can be registered, all of them are notified when
// the control state changes, and deregistering one stops its notifications
// while others still receive them.
TEST_F(ActorTabHelperTest, MultipleObserversAndDeregistration) {
  FakeActorTabHelperObserver observer1;
  FakeActorTabHelperObserver observer2;

  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation1(&observer1);
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation2(&observer2);

  scoped_observation1.Observe(tab_helper_);
  scoped_observation2.Observe(tab_helper_);

  EXPECT_FALSE(observer1.on_control_state_changed_called_);
  EXPECT_FALSE(observer2.on_control_state_changed_called_);

  // 1. Both observers should be notified of the control state change.
  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(observer1.on_control_state_changed_called_);
  EXPECT_EQ(observer1.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer1.last_previous_control_state_value_,
            ActorControlState::kInactive);
  EXPECT_EQ(observer1.last_new_control_state_value_,
            ActorControlState::kActorControlled);
  EXPECT_TRUE(observer2.on_control_state_changed_called_);
  EXPECT_EQ(observer2.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer2.last_previous_control_state_value_,
            ActorControlState::kInactive);
  EXPECT_EQ(observer2.last_new_control_state_value_,
            ActorControlState::kActorControlled);

  // Reset indicators.
  observer1.on_control_state_changed_called_ = false;
  observer2.on_control_state_changed_called_ = false;

  // 2. Deregister `observer1`.
  scoped_observation1.Reset();

  // 3. Change control state: only `observer2` should receive the notification.
  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(observer1.on_control_state_changed_called_);
  EXPECT_TRUE(observer2.on_control_state_changed_called_);
  EXPECT_EQ(observer2.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer2.last_previous_control_state_value_,
            ActorControlState::kActorControlled);
  EXPECT_EQ(observer2.last_new_control_state_value_,
            ActorControlState::kInactive);
}

// Test that callbacks registered via `AddControlStateChangedCallback` are
// notified when the control state changes and that unregistering stops
// notifications.
TEST_F(ActorTabHelperTest, ControlStateChanges_NotifiesSubscriptions) {
  base::test::TestFuture<ActorControlState, ActorControlState> future1;
  base::test::TestFuture<ActorControlState, ActorControlState> future2;

  base::CallbackListSubscription subscription1 =
      tab_helper_->AddControlStateChangedCallback(
          future1.GetRepeatingCallback());
  base::CallbackListSubscription subscription2 =
      tab_helper_->AddControlStateChangedCallback(
          future2.GetRepeatingCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  ASSERT_TRUE(future1.IsReady());
  EXPECT_EQ(future1.Take(),
            std::make_tuple(ActorControlState::kInactive,
                            ActorControlState::kActorControlled));
  ASSERT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Take(),
            std::make_tuple(ActorControlState::kInactive,
                            ActorControlState::kActorControlled));

  // Duplicate state changes do not notify subscribers.
  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Resetting `subscription1` stops its notifications while `subscription2`
  // remains active.
  subscription1 = {};

  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(future1.IsReady());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Take(), std::make_tuple(ActorControlState::kActorControlled,
                                            ActorControlState::kInactive));
}

// Test deprecated `IsActuating`, `AddActuationStateChangedCallback`, and
// `OnActuationStateChanged` compatibility with `SetControlState`.
TEST_F(ActorTabHelperTest, DeprecatedActuatingCompatibility) {
  EXPECT_FALSE(tab_helper_->IsActuating());

  FakeLegacyActorTabHelperObserver observer;
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(tab_helper_);

  base::test::TestFuture<bool> future;
  base::CallbackListSubscription subscription =
      tab_helper_->AddActuationStateChangedCallback(
          future.GetRepeatingCallback());

  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(tab_helper_->IsActuating());
  EXPECT_EQ(tab_helper_->GetControlState(),
            ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_TRUE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Take());

  observer.on_actuation_state_changed_called_ = false;
  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(tab_helper_->IsActuating());
  EXPECT_EQ(tab_helper_->GetControlState(), ActorControlState::kInactive);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_FALSE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Take());

  // Test deprecated `SetActuating` method.
  observer.on_actuation_state_changed_called_ = false;
  tab_helper_->SetActuating(true);
  EXPECT_TRUE(tab_helper_->IsActuating());
  EXPECT_EQ(tab_helper_->GetControlState(),
            ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_TRUE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Take());

  observer.on_actuation_state_changed_called_ = false;
  tab_helper_->SetActuating(false);
  EXPECT_FALSE(tab_helper_->IsActuating());
  EXPECT_EQ(tab_helper_->GetControlState(), ActorControlState::kInactive);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_FALSE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Take());
}

// Test streaming operator and string conversion for ActorControlState.
TEST_F(ActorTabHelperTest, ActorControlStateToStringAndStream) {
  EXPECT_EQ(actor::ActorControlStateToString(ActorControlState::kInactive),
            "kInactive");
  EXPECT_EQ(
      actor::ActorControlStateToString(ActorControlState::kActorControlled),
      "kActorControlled");
  EXPECT_EQ(
      actor::ActorControlStateToString(static_cast<ActorControlState>(999)),
      "kUnknown");

  std::ostringstream oss;
  oss << ActorControlState::kActorControlled;
  EXPECT_EQ(oss.str(), "kActorControlled");
}

// Test that keep-alive is derived from the control state: enabled when the tab
// becomes actor-controlled and disabled when it goes back to inactive.
TEST_F(ActorTabHelperTest, KeepRenderProcessAliveFollowsControlState) {
  EXPECT_FALSE(web_state_->keep_render_process_alive());

  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(web_state_->keep_render_process_alive());

  tab_helper_->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(web_state_->keep_render_process_alive());
}

// Test that keep-alive is re-applied asynchronously after the WebState is
// hidden and the browser synchronously resets keep-alive, as long as the tab
// is still controlled.
TEST_F(ActorTabHelperTest, KeepRenderProcessAliveReappliedAfterWasHidden) {
  tab_helper_->SetControlState(ActorControlState::kActorControlled);
  ASSERT_TRUE(web_state_->keep_render_process_alive());

  // Mimic the browser hiding the tab then immediately dropping keep-alive.
  web_state_->WasHidden();
  web_state_->SetKeepRenderProcessAlive(false);
  EXPECT_FALSE(web_state_->keep_render_process_alive());

  EXPECT_TRUE(base::test::RunUntil(
      [this]() { return web_state_->keep_render_process_alive(); }));
}

// Test that keep-alive is not re-applied after the WebState is hidden when the
// tab is inactive.
TEST_F(ActorTabHelperTest, KeepRenderProcessAliveNotReappliedWhenInactive) {
  ASSERT_EQ(tab_helper_->GetControlState(), ActorControlState::kInactive);

  web_state_->WasHidden();
  web_state_->SetKeepRenderProcessAlive(false);
  actor::FlushCurrentSequence();
  EXPECT_FALSE(web_state_->keep_render_process_alive());
}

// Test that keep-alive is not re-applied if the tab stops being controlled
// between being hidden and the posted re-apply task running.
TEST_F(ActorTabHelperTest,
       KeepRenderProcessAliveNotReappliedWhenDeactivatedBeforeTaskRuns) {
  tab_helper_->SetControlState(ActorControlState::kActorControlled);

  web_state_->WasHidden();
  web_state_->SetKeepRenderProcessAlive(false);
  tab_helper_->SetControlState(ActorControlState::kInactive);
  actor::FlushCurrentSequence();
  EXPECT_FALSE(web_state_->keep_render_process_alive());
}

// Test that the posted re-apply task is a no-op (and does not crash) when the
// WebState is destroyed before it runs.
TEST_F(ActorTabHelperTest, WebStateDestroyedBeforeReapplyTaskRuns) {
  tab_helper_->SetControlState(ActorControlState::kActorControlled);

  web_state_->WasHidden();
  tab_helper_ = nullptr;
  web_state_.reset();
  actor::FlushCurrentSequence();
}

}  // namespace
