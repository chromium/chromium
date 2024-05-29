// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/idle/idle_manager_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"
#include "ui/base/idle/idle_time_provider.h"
#include "ui/base/test/idle_test_utils.h"

using blink::mojom::IdleManagerError;
using blink::mojom::IdleStatePtr;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";

enum UserIdleState {
  kActive,
  kIdle,
};

enum ScreenIdleState {
  kUnlocked,
  kLocked,
};

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD2(Update, void(IdleStatePtr, bool));
};

class MockIdleTimeProvider : public ui::IdleTimeProvider {
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
        base::WrapUnique(permission_manager_.get()));

    idle_time_provider_ = new NiceMock<MockIdleTimeProvider>();
    idle_manager_ = std::make_unique<IdleManagerImpl>(main_rfh());
    scoped_idle_time_provider_ =
        std::make_unique<ui::test::ScopedIdleProviderForTest>(
            base::WrapUnique(idle_time_provider_.get()));
    idle_manager_->CreateService(service_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    permission_manager_ = nullptr;
    idle_time_provider_ = nullptr;
    scoped_idle_time_provider_.reset();
    idle_manager_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  IdleManagerImpl* GetIdleManager() { return idle_manager_.get(); }

  void SetPermissionStatus(blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatusForCurrentDocument(
                blink::PermissionType::IDLE_DETECTION, main_rfh(),
                /*should_include_device_status*/ false))
        .WillByDefault(Return(permission_status));
  }

  std::tuple<UserIdleState, ScreenIdleState> AddMonitorRequest() {
    base::test::TestFuture<IdleManagerError, IdleStatePtr> future;
    service_remote_->AddMonitor(monitor_receiver_.BindNewPipeAndPassRemote(),
                                future.GetCallback());
    EXPECT_EQ(IdleManagerError::kSuccess, future.Get<0>());
    return std::make_tuple(
        future.Get<1>()->idle_time.has_value() ? UserIdleState::kIdle
                                               : UserIdleState::kActive,
        future.Get<1>()->screen_locked ? ScreenIdleState::kLocked
                                       : ScreenIdleState::kUnlocked);
  }

  std::tuple<UserIdleState, ScreenIdleState> GetIdleStatus(
      bool expect_override) {
    base::RunLoop loop;
    IdleStatePtr result;

    EXPECT_CALL(idle_monitor_, Update(_, expect_override))
        .WillOnce(Invoke([&loop, &result](IdleStatePtr state,
                                          bool is_overridden_by_devtools) {
          result = std::move(state);
          loop.Quit();
        }));

    if (!expect_override) {
      // If we aren't expecting an override then we need to fast forward in
      // order to run the polling task.
      task_environment()->FastForwardBy(base::Seconds(1));
    }

    loop.Run();
    return std::make_tuple(result->idle_time.has_value()
                               ? UserIdleState::kIdle
                               : UserIdleState::kActive,
                           result->screen_locked ? ScreenIdleState::kLocked
                                                 : ScreenIdleState::kUnlocked);
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
  raw_ptr<MockPermissionManager> permission_manager_;
  raw_ptr<MockIdleTimeProvider> idle_time_provider_;
  std::unique_ptr<ui::test::ScopedIdleProviderForTest>
      scoped_idle_time_provider_;
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
            AddMonitorRequest());
}

TEST_F(IdleManagerTest, Idle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest());

  // Simulates a user going idle.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));

  // Simulates a user going active, calling a callback under the threshold.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus(/*expect_override=*/false));
}

TEST_F(IdleManagerTest, UnlockingScreen) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(70)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            AddMonitorRequest());

  // Simulates a user unlocking the screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus(/*expect_override=*/false));
}

TEST_F(IdleManagerTest, LockingScreen) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest());

  // Simulates a user locking the screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(10)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));
}

TEST_F(IdleManagerTest, LockingScreenThenIdle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest());

  // Simulates a user locking screen.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(10)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));

  // Simulates a user going idle, while the screen is still locked.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(70)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));
}

TEST_F(IdleManagerTest, LockingScreenAfterIdle) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  // Initial state of the system.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(0)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            AddMonitorRequest());

  // Simulates a user going idle, but with the screen still unlocked.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(false));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kUnlocked),
            GetIdleStatus(/*expect_override=*/false));

  // Simulates the screen getting locked by the system after the user goes
  // idle (e.g. screensaver kicks in first, throwing idleness, then getting
  // locked).
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));

  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));
}

TEST_F(IdleManagerTest, RemoveMonitorStopsPolling) {
  // Simulates the renderer disconnecting (e.g. on page reload) and verifies
  // that the polling stops for the idle detection.

  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);

  AddMonitorRequest();

  EXPECT_TRUE(ui::IdlePollingService::GetInstance()->IsPollingForTest());

  DisconnectRenderer();

  EXPECT_FALSE(ui::IdlePollingService::GetInstance()->IsPollingForTest());
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
      monitor_receiver.BindNewPipeAndPassRemote(),
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
            AddMonitorRequest());

  // Set overrides and verify overriden values returned.
  auto* impl = GetIdleManager();
  impl->SetIdleOverride(/*is_user_active=*/false, /*is_screen_unlocked=*/false);
  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/true));

  // Clear overrides and verify initial values returned.
  impl->ClearIdleOverride();
  EXPECT_EQ(std::make_tuple(UserIdleState::kActive, ScreenIdleState::kUnlocked),
            GetIdleStatus(/*expect_override=*/false));
}

}  // namespace content
