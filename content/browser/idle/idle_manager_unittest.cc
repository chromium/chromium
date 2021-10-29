// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/idle_time_provider.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/idle_test_utils.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"

using blink::mojom::IdleManagerError;
using blink::mojom::IdleMonitorPtr;
using blink::mojom::IdleStatePtr;
using blink::mojom::ScreenIdleState;
using blink::mojom::UserIdleState;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";

constexpr base::TimeDelta kThreshold = base::Seconds(60);

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD1(Update, void(IdleStatePtr));
};

class MockIdleTimeProvider : public IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;
  MockIdleTimeProvider(const MockIdleTimeProvider&) = delete;
  MockIdleTimeProvider& operator=(const MockIdleTimeProvider&) = delete;

  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());
};

class IdleManagerTest : public RenderViewHostTestHarness {
 protected:
  IdleManagerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdleManagerTest() override = default;
  IdleManagerTest(const IdleManagerTest&) = delete;
  IdleManagerTest& operator=(const IdleManagerTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    NavigateAndCommit(url_);

    permission_manager_ = new NiceMock<MockPermissionManager>();
    auto* test_browser_context =
        static_cast<TestBrowserContext*>(browser_context());
    test_browser_context->SetPermissionControllerDelegate(
        base::WrapUnique(permission_manager_));

    idle_time_provider_ = new NiceMock<MockIdleTimeProvider>();
    idle_manager_ = std::make_unique<IdleManagerImpl>(main_rfh());
    scoped_idle_time_provider_ = std::make_unique<ScopedIdleProviderForTest>(
        base::WrapUnique(idle_time_provider_));
    idle_manager_->CreateService(service_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    scoped_idle_time_provider_.reset();
    idle_manager_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  IdleManagerImpl* GetIdleManager() { return idle_manager_.get(); }

  void SetPermissionStatus(blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatusForFrame(PermissionType::IDLE_DETECTION,
                                        main_rfh(), url_))
        .WillByDefault(Return(permission_status));
  }

  std::tuple<UserIdleState, ScreenIdleState> AddMonitorRequest(
      base::TimeDelta threshold) {
    base::RunLoop loop;
    UserIdleState user_result;
    ScreenIdleState screen_result;

    service_remote_->AddMonitor(
        threshold, monitor_receiver_.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&loop, &user_result, &screen_result](IdleManagerError error,
                                                  IdleStatePtr state) {
              EXPECT_EQ(IdleManagerError::kSuccess, error);
              user_result = state->user;
              screen_result = state->screen;
              loop.Quit();
            }));
    loop.Run();
    return std::make_tuple(user_result, screen_result);
  }

  std::tuple<UserIdleState, ScreenIdleState> GetIdleStatus() {
    base::RunLoop loop;
    UserIdleState user_result;
    ScreenIdleState screen_result;

    EXPECT_CALL(idle_monitor_, Update(_))
        .WillOnce(
            Invoke([&loop, &user_result, &screen_result](IdleStatePtr state) {
              user_result = state->user;
              screen_result = state->screen;
              loop.Quit();
            }));

    // Fast forward to run polling task.
    task_environment()->FastForwardBy(base::Seconds(1));
    loop.Run();
    return std::make_tuple(user_result, screen_result);
  }

  void DisconnectRenderer() {
    base::RunLoop loop;

    // Simulates the renderer disconnecting.
    monitor_receiver_.reset();

    // Wait for the IdleManager to observe the pipe close.
    loop.RunUntilIdle();
  }

  MockIdleTimeProvider* idle_time_provider() const {
    return idle_time_provider_;
  }

 protected:
  mojo::Remote<blink::mojom::IdleManager> service_remote_;

 private:
  std::unique_ptr<IdleManagerImpl> idle_manager_;
  MockPermissionManager* permission_manager_;
  MockIdleTimeProvider* idle_time_provider_;
  std::unique_ptr<ScopedIdleProviderForTest> scoped_idle_time_provider_;
  NiceMock<MockIdleMonitor> idle_monitor_;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver_{&idle_monitor_};
  GURL url_ = GURL(kTestUrl);
};

}  // namespace

TEST_F(IdleManagerTest, AddMonitor) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));
}

TEST_F(IdleManagerTest, Idle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));

  // Simulates a user going idle.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus());

  // Simulates a user going active, calling a callback under the threshold.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus());
}

TEST_F(IdleManagerTest, UnlockingScreen) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(70)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            AddMonitorRequest(kThreshold));

  // Simulates a user unlocking the screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus());
}

TEST_F(IdleManagerTest, LockingScreen) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));

  // Simulates a user locking the screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(10)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kLocked),
            GetIdleStatus());
}

TEST_F(IdleManagerTest, LockingScreenThenIdle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));

  // Simulates a user locking screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(10)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kLocked),
            GetIdleStatus());

  // Simulates a user going idle, while the screen is still locked.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(70)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus());
}

TEST_F(IdleManagerTest, LockingScreenAfterIdle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));

  // Simulates a user going idle, but with the screen still unlocked.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kUnlocked),
            GetIdleStatus());

  // Simulates the screen getting locked by the system after the user goes
  // idle (e.g. screensaver kicks in first, throwing idleness, then getting
  // locked).
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus());
}

TEST_F(IdleManagerTest, RemoveMonitorStopsPolling) {
  // Simulates the renderer disconnecting (e.g. on page reload) and verifies
  // that the polling stops for the idle detection.

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  AddMonitorRequest(kThreshold);

  EXPECT_TRUE(IdlePollingService::GetInstance()->IsPollingForTest());

  DisconnectRenderer();

  EXPECT_FALSE(IdlePollingService::GetInstance()->IsPollingForTest());
}

TEST_F(IdleManagerTest, Threshold) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(90)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(
      AddMonitorRequest(base::Seconds(91)),
      std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked));
}

TEST_F(IdleManagerTest, InvalidThreshold) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  mojo::test::BadMessageObserver bad_message_observer;

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  // Should not start initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime()).Times(0);
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked()).Times(0);

  service_remote_->AddMonitor(base::Seconds(50),
                              monitor_receiver.BindNewPipeAndPassRemote(),
                              base::NullCallback());

  EXPECT_EQ("Minimum threshold is 1 minute.",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(IdleManagerTest, PermissionDenied) {
  SetPermissionStatus(blink::mojom::PermissionStatus::DENIED);

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  // Should not start initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime()).Times(0);
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked()).Times(0);

  base::RunLoop loop;
  service_remote_->AddMonitor(
      kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting(
          [&loop](IdleManagerError error, IdleStatePtr state) {
            EXPECT_EQ(IdleManagerError::kPermissionDisabled, error);
            EXPECT_FALSE(state);
            loop.Quit();
          }));
  loop.Run();
}

TEST_F(IdleManagerTest, SetAndClearOverrides) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Verify initial state without overrides.
  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest(kThreshold));

  // Set overrides and verify overriden values returned.
  auto* impl = GetIdleManager();
  impl->SetIdleOverride(UserIdleState::kIdle, ScreenIdleState::kLocked);
  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus());

  // Clear overrides and verify initial values returned.
  impl->ClearIdleOverride();
  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus());
}

}  // namespace content
