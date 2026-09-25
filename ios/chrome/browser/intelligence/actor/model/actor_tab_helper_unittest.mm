// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import <memory>
#import <sstream>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/test/test_future.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_control_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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

  std::unique_ptr<web::FakeWebState> web_state_;
};

void ActorTabHelperTest::SetUp() {
  PlatformTest::SetUp();
  web_state_ = std::make_unique<web::FakeWebState>();
  ActorTabHelper::CreateForWebState(web_state_.get());
}

// Test that the tab helper is created with control state set to kInactive by
// default.
TEST_F(ActorTabHelperTest, DefaultControlState) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kInactive);
}

// Test setting and getting the control state.
TEST_F(ActorTabHelperTest, SetAndGetControlState) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  helper->SetControlState(ActorControlState::kActorControlled);
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kActorControlled);

  helper->SetControlState(ActorControlState::kInactive);
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kInactive);
}

// Test that observers are notified when the control state changes.
TEST_F(ActorTabHelperTest, ObserverNotification) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  FakeActorTabHelperObserver observer;
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(helper);

  EXPECT_FALSE(observer.on_control_state_changed_called_);

  helper->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_control_state_changed_called_);
  EXPECT_EQ(observer.last_web_state_value_, web_state_.get());
  EXPECT_EQ(observer.last_previous_control_state_value_,
            ActorControlState::kInactive);
  EXPECT_EQ(observer.last_new_control_state_value_,
            ActorControlState::kActorControlled);

  observer.on_control_state_changed_called_ = false;
  helper->SetControlState(ActorControlState::kActorControlled);  // No change.
  EXPECT_FALSE(observer.on_control_state_changed_called_);

  helper->SetControlState(ActorControlState::kInactive);
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
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  FakeActorTabHelperObserver observer1;
  FakeActorTabHelperObserver observer2;

  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation1(&observer1);
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation2(&observer2);

  scoped_observation1.Observe(helper);
  scoped_observation2.Observe(helper);

  EXPECT_FALSE(observer1.on_control_state_changed_called_);
  EXPECT_FALSE(observer2.on_control_state_changed_called_);

  // 1. Both observers should be notified of the control state change.
  helper->SetControlState(ActorControlState::kActorControlled);
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
  helper->SetControlState(ActorControlState::kInactive);
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
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  base::test::TestFuture<ActorControlState, ActorControlState> future1;
  base::test::TestFuture<ActorControlState, ActorControlState> future2;

  base::CallbackListSubscription subscription1 =
      helper->AddControlStateChangedCallback(future1.GetRepeatingCallback());
  base::CallbackListSubscription subscription2 =
      helper->AddControlStateChangedCallback(future2.GetRepeatingCallback());

  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  helper->SetControlState(ActorControlState::kActorControlled);
  ASSERT_TRUE(future1.IsReady());
  EXPECT_EQ(future1.Take(),
            std::make_tuple(ActorControlState::kInactive,
                            ActorControlState::kActorControlled));
  ASSERT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Take(),
            std::make_tuple(ActorControlState::kInactive,
                            ActorControlState::kActorControlled));

  // Duplicate state changes do not notify subscribers.
  helper->SetControlState(ActorControlState::kActorControlled);
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Resetting `subscription1` stops its notifications while `subscription2`
  // remains active.
  subscription1 = {};

  helper->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(future1.IsReady());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Take(), std::make_tuple(ActorControlState::kActorControlled,
                                            ActorControlState::kInactive));
}

// Test deprecated `IsActuating`, `AddActuationStateChangedCallback`, and
// `OnActuationStateChanged` compatibility with `SetControlState`.
TEST_F(ActorTabHelperTest, DeprecatedActuatingCompatibility) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->IsActuating());

  FakeLegacyActorTabHelperObserver observer;
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(helper);

  base::test::TestFuture<bool> future;
  base::CallbackListSubscription subscription =
      helper->AddActuationStateChangedCallback(future.GetRepeatingCallback());

  helper->SetControlState(ActorControlState::kActorControlled);
  EXPECT_TRUE(helper->IsActuating());
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_TRUE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Take());

  observer.on_actuation_state_changed_called_ = false;
  helper->SetControlState(ActorControlState::kInactive);
  EXPECT_FALSE(helper->IsActuating());
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kInactive);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_FALSE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Take());

  // Test deprecated `SetActuating` method.
  observer.on_actuation_state_changed_called_ = false;
  helper->SetActuating(true);
  EXPECT_TRUE(helper->IsActuating());
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kActorControlled);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_TRUE(observer.last_actuating_value_);
  ASSERT_TRUE(future.IsReady());
  EXPECT_TRUE(future.Take());

  observer.on_actuation_state_changed_called_ = false;
  helper->SetActuating(false);
  EXPECT_FALSE(helper->IsActuating());
  EXPECT_EQ(helper->GetControlState(), ActorControlState::kInactive);
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

}  // namespace
