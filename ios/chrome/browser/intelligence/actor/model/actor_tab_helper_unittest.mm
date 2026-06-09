// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_tab_helper_observer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class FakeActorTabHelperObserver : public ActorTabHelperObserver {
 public:
  FakeActorTabHelperObserver() = default;
  FakeActorTabHelperObserver(const FakeActorTabHelperObserver&) = delete;
  FakeActorTabHelperObserver& operator=(const FakeActorTabHelperObserver&) =
      delete;
  FakeActorTabHelperObserver(FakeActorTabHelperObserver&&) = delete;
  FakeActorTabHelperObserver& operator=(FakeActorTabHelperObserver&&) = delete;

  // ActorTabHelperObserver:
  void OnActuationStateChanged(ActorTabHelper* tab_helper,
                               web::WebState* web_state,
                               bool actuating) override;

  bool on_actuation_state_changed_called_ = false;
  raw_ptr<web::WebState> last_web_state_value_ = nullptr;
  bool last_actuating_value_ = false;
};

void FakeActorTabHelperObserver::OnActuationStateChanged(
    ActorTabHelper* tab_helper,
    web::WebState* web_state,
    bool actuating) {
  on_actuation_state_changed_called_ = true;
  last_web_state_value_ = web_state;
  last_actuating_value_ = actuating;
}

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

// Test that the tab helper is created with actuating set to false by default.
TEST_F(ActorTabHelperTest, DefaultActuatingState) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_FALSE(helper->IsActuating());
}

// Test setting and getting the actuating state.
TEST_F(ActorTabHelperTest, SetAndGetActuatingState) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  helper->SetActuating(true);
  EXPECT_TRUE(helper->IsActuating());

  helper->SetActuating(false);
  EXPECT_FALSE(helper->IsActuating());
}

// Test that observers are notified when the actuating state changes.
TEST_F(ActorTabHelperTest, ObserverNotification) {
  ActorTabHelper* helper = ActorTabHelper::FromWebState(web_state_.get());
  ASSERT_NE(helper, nullptr);

  FakeActorTabHelperObserver observer;
  base::ScopedObservation<ActorTabHelper, ActorTabHelperObserver>
      scoped_observation(&observer);
  scoped_observation.Observe(helper);

  EXPECT_FALSE(observer.on_actuation_state_changed_called_);

  helper->SetActuating(true);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_EQ(observer.last_web_state_value_, web_state_.get());
  EXPECT_TRUE(observer.last_actuating_value_);

  observer.on_actuation_state_changed_called_ = false;
  helper->SetActuating(true);  // No change.
  EXPECT_FALSE(observer.on_actuation_state_changed_called_);

  helper->SetActuating(false);
  EXPECT_TRUE(observer.on_actuation_state_changed_called_);
  EXPECT_EQ(observer.last_web_state_value_, web_state_.get());
  EXPECT_FALSE(observer.last_actuating_value_);
}

// Test that multiple observers can be registered, all of them are notified when
// the state changes, and deregistering one stops its notifications while others
// still receive them.
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

  EXPECT_FALSE(observer1.on_actuation_state_changed_called_);
  EXPECT_FALSE(observer2.on_actuation_state_changed_called_);

  // 1. Both observers should be notified of the actuation state change.
  helper->SetActuating(true);
  EXPECT_TRUE(observer1.on_actuation_state_changed_called_);
  EXPECT_EQ(observer1.last_web_state_value_, web_state_.get());
  EXPECT_TRUE(observer1.last_actuating_value_);
  EXPECT_TRUE(observer2.on_actuation_state_changed_called_);
  EXPECT_EQ(observer2.last_web_state_value_, web_state_.get());
  EXPECT_TRUE(observer2.last_actuating_value_);

  // Reset indicators.
  observer1.on_actuation_state_changed_called_ = false;
  observer2.on_actuation_state_changed_called_ = false;

  // 2. Deregister `observer1`.
  scoped_observation1.Reset();

  // 3. Change state: only `observer2` should receive the notification.
  helper->SetActuating(false);
  EXPECT_FALSE(observer1.on_actuation_state_changed_called_);
  EXPECT_TRUE(observer2.on_actuation_state_changed_called_);
  EXPECT_EQ(observer2.last_web_state_value_, web_state_.get());
  EXPECT_FALSE(observer2.last_actuating_value_);
}

}  // namespace
