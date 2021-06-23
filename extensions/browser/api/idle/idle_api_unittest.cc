// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/idle/idle_api.h"

#include <limits.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "extensions/browser/api/idle/idle_api_constants.h"
#include "extensions/browser/api/idle/idle_manager.h"
#include "extensions/browser/api/idle/idle_manager_factory.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/api/idle.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace idle = extensions::api::idle;

namespace extensions {

namespace {

class MockEventDelegate : public IdleManager::EventDelegate {
 public:
  MockEventDelegate() {}
  ~MockEventDelegate() override {}
  MOCK_METHOD2(OnStateChanged, void(const std::string&, ui::IdleState));
  void RegisterObserver(EventRouter::Observer* observer) override {}
  void UnregisterObserver(EventRouter::Observer* observer) override {}
};

class TestIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  TestIdleProvider();
  ~TestIdleProvider() override;
  ui::IdleState CalculateIdleState(int idle_threshold) override;
  int CalculateIdleTime() override;
  bool CheckIdleStateIsLocked() override;

  void set_idle_time(int idle_time);
  void set_locked(bool locked);

 private:
  int idle_time_;
  bool locked_;
};

TestIdleProvider::TestIdleProvider() : idle_time_(0), locked_(false) {
}

TestIdleProvider::~TestIdleProvider() {
}

ui::IdleState TestIdleProvider::CalculateIdleState(int idle_threshold) {
  if (locked_) {
    return ui::IDLE_STATE_LOCKED;
  } else if (idle_time_ >= idle_threshold) {
    return ui::IDLE_STATE_IDLE;
  } else {
    return ui::IDLE_STATE_ACTIVE;
  }
}

int TestIdleProvider::CalculateIdleTime() {
  return idle_time_;
}

bool TestIdleProvider::CheckIdleStateIsLocked() {
  return locked_;
}

void TestIdleProvider::set_idle_time(int idle_time) {
  idle_time_ = idle_time;
}

void TestIdleProvider::set_locked(bool locked) {
  locked_ = locked;
}

class ScopedListen {
 public:
  ScopedListen(IdleManager* idle_manager, const std::string& extension_id);
  ~ScopedListen();

 private:
  IdleManager* idle_manager_;
  const std::string extension_id_;
};

ScopedListen::ScopedListen(IdleManager* idle_manager,
                           const std::string& extension_id)
    : idle_manager_(idle_manager), extension_id_(extension_id) {
  const EventListenerInfo details(idle::OnStateChanged::kEventName,
                                  extension_id_, GURL(), NULL);
  idle_manager_->OnListenerAdded(details);
}

ScopedListen::~ScopedListen() {
  const EventListenerInfo details(idle::OnStateChanged::kEventName,
                                  extension_id_, GURL(), NULL);
  idle_manager_->OnListenerRemoved(details);
}

std::unique_ptr<KeyedService> IdleManagerTestFactory(
    content::BrowserContext* context) {
  return std::make_unique<IdleManager>(context);
}

}  // namespace

class IdleTest : public ApiUnitTest {
 public:
  void SetUp() override;

 protected:
  IdleManager* idle_manager_;
  TestIdleProvider* idle_provider_;
  testing::StrictMock<MockEventDelegate>* event_delegate_;
};

void IdleTest::SetUp() {
  ApiUnitTest::SetUp();

  IdleManagerFactory::GetInstance()->SetTestingFactory(
      browser_context(), base::BindRepeating(&IdleManagerTestFactory));
  idle_manager_ = IdleManagerFactory::GetForBrowserContext(browser_context());

  idle_provider_ = new TestIdleProvider();
  idle_manager_->SetIdleTimeProviderForTest(
      std::unique_ptr<IdleManager::IdleTimeProvider>(idle_provider_));
  event_delegate_ = new testing::StrictMock<MockEventDelegate>();
  idle_manager_->SetEventDelegateForTest(
      std::unique_ptr<IdleManager::EventDelegate>(event_delegate_));
  idle_manager_->Init();
}

// Verifies that "locked" takes priority over "active".
TEST_F(IdleTest, QueryLockedActive) {
  idle_provider_->set_locked(true);
  idle_provider_->set_idle_time(0);

  std::unique_ptr<base::Value> result(
      RunFunctionAndReturnValue(new IdleQueryStateFunction(), "[60]"));

  std::string idle_state;
  ASSERT_TRUE(result->GetAsString(&idle_state));
  EXPECT_EQ("locked", idle_state);
}

// Verifies that "locked" takes priority over "idle".
TEST_F(IdleTest, QueryLockedIdle) {
  idle_provider_->set_locked(true);
  idle_provider_->set_idle_time(INT_MAX);

  std::unique_ptr<base::Value> result(
      RunFunctionAndReturnValue(new IdleQueryStateFunction(), "[60]"));

  std::string idle_state;
  ASSERT_TRUE(result->GetAsString(&idle_state));
  EXPECT_EQ("locked", idle_state);
}

// Verifies that any amount of idle time less than the detection interval
// translates to a state of "active".
TEST_F(IdleTest, QueryActive) {
  idle_provider_->set_locked(false);

  for (int time = 0; time < 60; ++time) {
    SCOPED_TRACE(time);
    idle_provider_->set_idle_time(time);

    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new IdleQueryStateFunction(), "[60]"));

    std::string idle_state;
    ASSERT_TRUE(result->GetAsString(&idle_state));
    EXPECT_EQ("active", idle_state);
  }
}

// Verifies that an idle time >= the detection interval returns the "idle"
// state.
TEST_F(IdleTest, QueryIdle) {
  idle_provider_->set_locked(false);

  for (int time = 80; time >= 60; --time) {
    SCOPED_TRACE(time);
    idle_provider_->set_idle_time(time);

    std::unique_ptr<base::Value> result(
        RunFunctionAndReturnValue(new IdleQueryStateFunction(), "[60]"));

    std::string idle_state;
    ASSERT_TRUE(result->GetAsString(&idle_state));
    EXPECT_EQ("idle", idle_state);
  }
}

// Verifies that requesting a detection interval < 15 has the same effect as
// passing in 15.
TEST_F(IdleTest, QueryMinThreshold) {
  idle_provider_->set_locked(false);

  for (int threshold = 0; threshold < 20; ++threshold) {
    for (int time = 10; time < 60; ++time) {
      SCOPED_TRACE(threshold);
      SCOPED_TRACE(time);
      idle_provider_->set_idle_time(time);

      std::string args = "[" + base::NumberToString(threshold) + "]";
      std::unique_ptr<base::Value> result(
          RunFunctionAndReturnValue(new IdleQueryStateFunction(), args));

      std::string idle_state;
      ASSERT_TRUE(result->GetAsString(&idle_state));

      int real_threshold = (threshold < 15) ? 15 : threshold;
      const char* expected = (time < real_threshold) ? "active" : "idle";
      EXPECT_EQ(expected, idle_state);
    }
  }
}

// Verifies that passing in a detection interval > 4 hours has the same effect
// as passing in 4 hours.
TEST_F(IdleTest, QueryMaxThreshold) {
  idle_provider_->set_locked(false);

  const int kFourHoursInSeconds = 4 * 60 * 60;

  for (int threshold = kFourHoursInSeconds - 20;
       threshold < (kFourHoursInSeconds + 20); ++threshold) {
    for (int time = kFourHoursInSeconds - 30; time < kFourHoursInSeconds + 30;
         ++time) {
      SCOPED_TRACE(threshold);
      SCOPED_TRACE(time);
      idle_provider_->set_idle_time(time);

      std::string args = "[" + base::NumberToString(threshold) + "]";
      std::unique_ptr<base::Value> result(
          RunFunctionAndReturnValue(new IdleQueryStateFunction(), args));

      std::string idle_state;
      ASSERT_TRUE(result->GetAsString(&idle_state));

      int real_threshold =
          (threshold > kFourHoursInSeconds) ? kFourHoursInSeconds : threshold;
      const char* expected = (time < real_threshold) ? "active" : "idle";
      EXPECT_EQ(expected, idle_state);
    }
  }
}

// Verifies that transitioning from an active to idle state fires an "idle"
// OnStateChanged event.
TEST_F(IdleTest, ActiveToIdle) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(false);

  for (int time = 0; time < 60; ++time) {
    SCOPED_TRACE(time);
    idle_provider_->set_idle_time(time);

    idle_manager_->UpdateIdleState();
  }

  idle_provider_->set_idle_time(60);

  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  for (int time = 61; time < 75; ++time) {
    SCOPED_TRACE(time);
    idle_provider_->set_idle_time(time);
    idle_manager_->UpdateIdleState();
  }
}

// Verifies that locking an active system generates a "locked" event.
TEST_F(IdleTest, ActiveToLocked) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(true);
  idle_provider_->set_idle_time(5);

  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();
}

// Verifies that transitioning from an idle to active state generates an
// "active" event.
TEST_F(IdleTest, IdleToActive) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(75);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  idle_provider_->set_idle_time(0);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_ACTIVE));
  idle_manager_->UpdateIdleState();
}

// Verifies that locking an idle system generates a "locked" event.
TEST_F(IdleTest, IdleToLocked) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(75);

  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  idle_provider_->set_locked(true);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();
}

// Verifies that unlocking an active system generates an "active" event.
TEST_F(IdleTest, LockedToActive) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(true);
  idle_provider_->set_idle_time(0);

  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(5);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_ACTIVE));
  idle_manager_->UpdateIdleState();
}

// Verifies that unlocking an inactive system generates an "idle" event.
TEST_F(IdleTest, LockedToIdle) {
  ScopedListen listen_test(idle_manager_, "test");

  idle_provider_->set_locked(true);
  idle_provider_->set_idle_time(75);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  idle_provider_->set_locked(false);
  EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
}

// Verifies that events are routed to extensions that have one or more listeners
// in scope.
TEST_F(IdleTest, MultipleExtensions) {
  ScopedListen listen_1(idle_manager_, "1");
  ScopedListen listen_2(idle_manager_, "2");

  idle_provider_->set_locked(true);
  EXPECT_CALL(*event_delegate_, OnStateChanged("1", ui::IDLE_STATE_LOCKED));
  EXPECT_CALL(*event_delegate_, OnStateChanged("2", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  {
    ScopedListen listen_2prime(idle_manager_, "2");
    ScopedListen listen_3(idle_manager_, "3");
    idle_provider_->set_locked(false);
    EXPECT_CALL(*event_delegate_, OnStateChanged("1", ui::IDLE_STATE_ACTIVE));
    EXPECT_CALL(*event_delegate_, OnStateChanged("2", ui::IDLE_STATE_ACTIVE));
    EXPECT_CALL(*event_delegate_, OnStateChanged("3", ui::IDLE_STATE_ACTIVE));
    idle_manager_->UpdateIdleState();
    testing::Mock::VerifyAndClearExpectations(event_delegate_);
  }

  idle_provider_->set_locked(true);
  EXPECT_CALL(*event_delegate_, OnStateChanged("1", ui::IDLE_STATE_LOCKED));
  EXPECT_CALL(*event_delegate_, OnStateChanged("2", ui::IDLE_STATE_LOCKED));
  idle_manager_->UpdateIdleState();
}

// Verifies that setDetectionInterval changes the detection interval from the
// default of 60 seconds, and that the call only affects a single extension's
// IdleMonitor.
TEST_F(IdleTest, SetDetectionInterval) {
  ScopedListen listen_default(idle_manager_, "default");
  ScopedListen listen_extension(idle_manager_, extension()->id());

  std::unique_ptr<base::Value> result45(RunFunctionAndReturnValue(
      new IdleSetDetectionIntervalFunction(), "[45]"));

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(44);
  idle_manager_->UpdateIdleState();

  idle_provider_->set_idle_time(45);
  EXPECT_CALL(*event_delegate_,
              OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
  // Verify that the expectation has been fulfilled before incrementing the
  // time again.
  testing::Mock::VerifyAndClearExpectations(event_delegate_);

  idle_provider_->set_idle_time(60);
  EXPECT_CALL(*event_delegate_, OnStateChanged("default", ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
}

// Verifies that setting the detection interval before creating the listener
// works correctly.
TEST_F(IdleTest, SetDetectionIntervalBeforeListener) {
  std::unique_ptr<base::Value> result45(RunFunctionAndReturnValue(
      new IdleSetDetectionIntervalFunction(), "[45]"));

  ScopedListen listen_extension(idle_manager_, extension()->id());

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(44);
  idle_manager_->UpdateIdleState();

  idle_provider_->set_idle_time(45);
  EXPECT_CALL(*event_delegate_,
              OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
}

// Verifies that setting a detection interval above the maximum value results
// in an interval of 4 hours.
TEST_F(IdleTest, SetDetectionIntervalMaximum) {
  ScopedListen listen_extension(idle_manager_, extension()->id());

  std::unique_ptr<base::Value> result(
      RunFunctionAndReturnValue(new IdleSetDetectionIntervalFunction(),
                                "[18000]"));  // five hours in seconds

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(4 * 60 * 60 - 1);
  idle_manager_->UpdateIdleState();

  idle_provider_->set_idle_time(4 * 60 * 60);
  EXPECT_CALL(*event_delegate_,
              OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
}

// Verifies that setting a detection interval below the minimum value results
// in an interval of 15 seconds.
TEST_F(IdleTest, SetDetectionIntervalMinimum) {
  ScopedListen listen_extension(idle_manager_, extension()->id());

  std::unique_ptr<base::Value> result(RunFunctionAndReturnValue(
      new IdleSetDetectionIntervalFunction(), "[10]"));

  idle_provider_->set_locked(false);
  idle_provider_->set_idle_time(14);
  idle_manager_->UpdateIdleState();

  idle_provider_->set_idle_time(15);
  EXPECT_CALL(*event_delegate_,
              OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
  idle_manager_->UpdateIdleState();
}

// Verifies that an extension's detection interval is discarded when it unloads.
TEST_F(IdleTest, UnloadCleanup) {
  {
    ScopedListen listen(idle_manager_, extension()->id());

    std::unique_ptr<base::Value> result45(RunFunctionAndReturnValue(
        new IdleSetDetectionIntervalFunction(), "[15]"));
  }

  // Listener count dropping to zero does not reset threshold.

  {
    ScopedListen listen(idle_manager_, extension()->id());
    idle_provider_->set_idle_time(16);
    EXPECT_CALL(*event_delegate_,
                OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
    idle_manager_->UpdateIdleState();
    testing::Mock::VerifyAndClearExpectations(event_delegate_);
  }

  // Threshold will reset after unload (and listen count == 0)
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  registry->TriggerOnUnloaded(extension(), UnloadedExtensionReason::UNINSTALL);

  {
    ScopedListen listen(idle_manager_, extension()->id());
    idle_manager_->UpdateIdleState();
    testing::Mock::VerifyAndClearExpectations(event_delegate_);

    idle_provider_->set_idle_time(61);
    EXPECT_CALL(*event_delegate_,
                OnStateChanged(extension()->id(), ui::IDLE_STATE_IDLE));
    idle_manager_->UpdateIdleState();
  }
}

// Verifies that unloading an extension with no listeners or threshold works.
TEST_F(IdleTest, UnloadOnly) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  registry->TriggerOnUnloaded(extension(), UnloadedExtensionReason::UNINSTALL);
}

// Verifies that its ok for the unload notification to happen before all the
// listener removals.
TEST_F(IdleTest, UnloadWhileListening) {
  ScopedListen listen(idle_manager_, extension()->id());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  registry->TriggerOnUnloaded(extension(), UnloadedExtensionReason::UNINSTALL);
}

// Verifies that re-adding a listener after a state change doesn't immediately
// fire a change event. Regression test for http://crbug.com/366580.
TEST_F(IdleTest, ReAddListener) {
  idle_provider_->set_locked(false);

  {
    // Fire idle event.
    ScopedListen listen(idle_manager_, "test");
    idle_provider_->set_idle_time(60);
    EXPECT_CALL(*event_delegate_, OnStateChanged("test", ui::IDLE_STATE_IDLE));
    idle_manager_->UpdateIdleState();
    testing::Mock::VerifyAndClearExpectations(event_delegate_);
  }

  // Trigger active.
  idle_provider_->set_idle_time(0);
  idle_manager_->UpdateIdleState();

  {
    // Nothing should have fired, the listener wasn't added until afterward.
    ScopedListen listen(idle_manager_, "test");
    idle_manager_->UpdateIdleState();
    testing::Mock::VerifyAndClearExpectations(event_delegate_);
  }
}

}  // namespace extensions
