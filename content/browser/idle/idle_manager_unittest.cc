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
#include "content/public/browser/permission_result.h"
#include "content/public/browser/permission_status_subscription.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
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
using ::testing::NiceMock;
using ::testing::Return;

MATCHER_P(PermissionTypeMatcher, id, "") {
  return ::testing::Matches(::testing::Eq(id))(
      blink::PermissionDescriptorToPermissionType(arg));
}

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
  MOCK_METHOD(void, Update, (IdleStatePtr, bool));
};

class MockIdleTimeProvider : public ui::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;
  MockIdleTimeProvider(const MockIdleTimeProvider&) = delete;
  MockIdleTimeProvider& operator=(const MockIdleTimeProvider&) = delete;

  MOCK_METHOD(base::TimeDelta, CalculateIdleTime, ());
  MOCK_METHOD(bool, CheckIdleStateIsLocked, ());
};

class TestPermissionManager : public MockPermissionManager {
 public:
  TestPermissionManager() = default;
  ~TestPermissionManager() override = default;

  // Runs the callback of every `permission` subscription, as the embedder does
  // when the user changes the setting.
  void NotifyPermissionChange(blink::PermissionType permission,
                              blink::mojom::PermissionStatus status) {
    if (!subscriptions()) {
      return;
    }
    for (PermissionController::SubscriptionsMap::iterator iter(subscriptions());
         !iter.IsAtEnd(); iter.Advance()) {
      PermissionResultSubscription* subscription = iter.GetCurrentValue();
      if (blink::PermissionDescriptorToPermissionType(
              subscription->permission_descriptor) == permission) {
        subscription->callback.Run(PermissionResult(status),
                                   /*ignore_status_override=*/false);
      }
    }
  }
};

class IdleManagerTest : public RenderViewHostTestHarness {
 public:
  IdleManagerTest(const IdleManagerTest&) = delete;
  IdleManagerTest& operator=(const IdleManagerTest&) = delete;

 protected:
  IdleManagerTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~IdleManagerTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    NavigateAndCommit(url_);

    permission_manager_ = new NiceMock<TestPermissionManager>();
    auto* test_browser_context =
        static_cast<TestBrowserContext*>(browser_context());
    test_browser_context->SetPermissionControllerDelegate(
        base::WrapUnique(permission_manager_.get()));

    idle_time_provider_ = new NiceMock<MockIdleTimeProvider>();
    scoped_idle_time_provider_ =
        std::make_unique<ui::test::ScopedIdleProviderForTest>(
            base::WrapUnique(idle_time_provider_.get()));
    InitIdleManager(main_rfh());
  }

  void InitIdleManager(RenderFrameHost* rfh) {
    service_remote_.reset();
    idle_manager_ = std::make_unique<IdleManagerImpl>(rfh);
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
    SetPermissionStatusForFrame(main_rfh(), permission_status);
  }

  void SetPermissionStatusForFrame(
      RenderFrameHost* rfh,
      blink::mojom::PermissionStatus permission_status) {
    ON_CALL(
        *permission_manager_,
        GetPermissionResultForCurrentDocument(
            PermissionTypeMatcher(blink::PermissionType::IDLE_DETECTION), rfh,
            /*should_include_device_status*/ false))
        .WillByDefault(Return(PermissionResult(permission_status)));
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
        .WillOnce([&loop, &result](IdleStatePtr state,
                                   bool is_overridden_by_devtools) {
          result = std::move(state);
          loop.Quit();
        });

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

  void SetMonitorDisconnectHandler(base::OnceClosure handler) {
    monitor_receiver_.set_disconnect_handler(std::move(handler));
  }

  void NotifyPermissionStatusChange(
      blink::mojom::PermissionStatus permission_status) {
    SetPermissionStatus(permission_status);
    permission_manager_->NotifyPermissionChange(
        blink::PermissionType::IDLE_DETECTION, permission_status);
  }

 protected:
  mojo::Remote<blink::mojom::IdleManager> service_remote_;

 private:
  std::unique_ptr<IdleManagerImpl> idle_manager_;
  raw_ptr<TestPermissionManager> permission_manager_;
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

TEST_F(IdleManagerTest, PermissionRevokedDisconnectsMonitors) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  AddMonitorRequest();
  EXPECT_TRUE(ui::IdlePollingService::GetInstance()->IsPollingForTest());

  base::test::TestFuture<void> disconnected;
  SetMonitorDisconnectHandler(disconnected.GetCallback());

  NotifyPermissionStatusChange(blink::mojom::PermissionStatus::DENIED);

  EXPECT_TRUE(disconnected.Wait());
  EXPECT_FALSE(ui::IdlePollingService::GetInstance()->IsPollingForTest());
}

TEST_F(IdleManagerTest, PermissionRegrantedKeepsMonitors) {
  SetPermissionStatus(blink::mojom::PermissionStatus::GRANTED);
  AddMonitorRequest();

  NotifyPermissionStatusChange(blink::mojom::PermissionStatus::GRANTED);
  EXPECT_TRUE(ui::IdlePollingService::GetInstance()->IsPollingForTest());

  // The monitor still receives updates.
  EXPECT_CALL(*idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  EXPECT_CALL(*idle_time_provider(), CheckIdleStateIsLocked())
      .WillOnce(Return(true));
  EXPECT_EQ(std::make_tuple(UserIdleState::kIdle, ScreenIdleState::kLocked),
            GetIdleStatus(/*expect_override=*/false));
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

TEST_F(IdleManagerTest, OpaqueOriginPermissionDenied) {
  // Navigate to an opaque origin (data: URL).
  NavigateAndCommit(GURL("data:text/html,<html></html>"));
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  // Re-initialize the IdleManager for the new main_rfh() to avoid dangling
  // pointers from the destroyed initial RenderFrameHost.
  InitIdleManager(main_rfh());

  // Explicitly set the mock permission status to GRANTED for the new RFH.
  // The opaque origin check in HasPermission() should reject the request
  // regardless of the permission manager returning GRANTED.
  SetPermissionStatusForFrame(main_rfh(),
                              blink::mojom::PermissionStatus::GRANTED);

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::test::TestFuture<IdleManagerError, IdleStatePtr> future;
  service_remote_->AddMonitor(monitor_receiver.BindNewPipeAndPassRemote(),
                              future.GetCallback());
  EXPECT_EQ(IdleManagerError::kPermissionDisabled, future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
}

TEST_F(IdleManagerTest, SubframeInOpaqueOriginMainFramePermissionDenied) {
  // Navigate top-level frame to an opaque origin (data: URL).
  NavigateAndCommit(GURL("data:text/html,<html></html>"));
  ASSERT_TRUE(main_rfh()->GetLastCommittedOrigin().opaque());

  // Create a child subframe with a non-opaque origin.
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("child_subframe");
  subframe = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/"), subframe);
  ASSERT_FALSE(subframe->GetLastCommittedOrigin().opaque());
  ASSERT_TRUE(
      subframe->GetOutermostMainFrame()->GetLastCommittedOrigin().opaque());

  InitIdleManager(subframe);

  // Explicitly set the mock permission status to GRANTED for the subframe.
  // The ancestor opaque origin check in HasPermission() should reject the
  // request to prevent visible URL fallback.
  SetPermissionStatusForFrame(subframe,
                              blink::mojom::PermissionStatus::GRANTED);

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::test::TestFuture<IdleManagerError, IdleStatePtr> future;
  service_remote_->AddMonitor(monitor_receiver.BindNewPipeAndPassRemote(),
                              future.GetCallback());
  EXPECT_EQ(IdleManagerError::kPermissionDisabled, future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
}

TEST_F(IdleManagerTest, OpaqueSubframeInNormalMainFramePermissionDenied) {
  // Top-level main frame is normal/non-opaque (kTestUrl).
  ASSERT_FALSE(main_rfh()->GetLastCommittedOrigin().opaque());

  // Create an opaque child subframe (data: URL).
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("opaque_subframe");
  subframe = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,<html></html>"), subframe);
  ASSERT_TRUE(subframe->GetLastCommittedOrigin().opaque());
  ASSERT_FALSE(
      subframe->GetOutermostMainFrame()->GetLastCommittedOrigin().opaque());

  InitIdleManager(subframe);
  SetPermissionStatusForFrame(subframe,
                              blink::mojom::PermissionStatus::GRANTED);

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::test::TestFuture<IdleManagerError, IdleStatePtr> future;
  service_remote_->AddMonitor(monitor_receiver.BindNewPipeAndPassRemote(),
                              future.GetCallback());
  EXPECT_EQ(IdleManagerError::kPermissionDisabled, future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
}

TEST_F(IdleManagerTest, NestedSubframeUnderOpaqueAncestorPermissionDenied) {
  // Top-level main frame is normal/non-opaque (kTestUrl).
  ASSERT_FALSE(main_rfh()->GetLastCommittedOrigin().opaque());

  // Create an intermediate opaque child frame (data: URL).
  RenderFrameHost* opaque_intermediate =
      RenderFrameHostTester::For(main_rfh())->AppendChild("intermediate_frame");
  opaque_intermediate = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("data:text/html,<html></html>"), opaque_intermediate);
  ASSERT_TRUE(opaque_intermediate->GetLastCommittedOrigin().opaque());

  // Create a non-opaque grandchild subframe inside the opaque intermediate
  // frame.
  RenderFrameHost* non_opaque_child =
      RenderFrameHostTester::For(opaque_intermediate)
          ->AppendChild("child_frame");
  non_opaque_child = NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/"), non_opaque_child);
  ASSERT_FALSE(non_opaque_child->GetLastCommittedOrigin().opaque());
  ASSERT_FALSE(non_opaque_child->GetOutermostMainFrame()
                   ->GetLastCommittedOrigin()
                   .opaque());

  InitIdleManager(non_opaque_child);
  SetPermissionStatusForFrame(non_opaque_child,
                              blink::mojom::PermissionStatus::GRANTED);

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::test::TestFuture<IdleManagerError, IdleStatePtr> future;
  service_remote_->AddMonitor(monitor_receiver.BindNewPipeAndPassRemote(),
                              future.GetCallback());
  EXPECT_EQ(IdleManagerError::kPermissionDisabled, future.Get<0>());
  EXPECT_FALSE(future.Get<1>());
}

}  // namespace content
