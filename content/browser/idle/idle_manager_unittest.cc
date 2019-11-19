// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/idle/idle_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom.h"

using blink::mojom::IdleManagerPtr;
using blink::mojom::IdleMonitorPtr;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr base::TimeDelta kThreshold = base::TimeDelta::FromSeconds(60);

class MockIdleMonitor : public blink::mojom::IdleMonitor {
 public:
  MOCK_METHOD1(Update, void(blink::mojom::IdleStatePtr));
};

class MockIdleTimeProvider : public IdleManager::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;
  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD1(CalculateIdleState, ui::IdleState(base::TimeDelta));
  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIdleTimeProvider);
};

class IdleManagerTest : public RenderViewHostImplTestHarness {
 protected:
  IdleManagerTest() {}

  ~IdleManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IdleManagerTest);
};

}  // namespace

TEST_F(IdleManagerTest, AddMonitor) {
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::RunLoop loop;

  service_remote.set_disconnect_handler(base::BindLambdaForTesting([&]() {
    ADD_FAILURE() << "Unexpected connection error";

    loop.Quit();
  }));

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked())
      .WillRepeatedly(testing::Return(false));

  service_remote->AddMonitor(
      kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindOnce(
          [](base::OnceClosure callback, blink::mojom::IdleStatePtr state) {
            // The initial state of the status of the user is to be active.
            EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
            EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
            std::move(callback).Run();
          },
          loop.QuitClosure()));

  loop.Run();
}

TEST_F(IdleManagerTest, Idle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;
    // Initial state of the system.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          loop.Quit();
        }));
    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going active, calling a callback under the threshold.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));

    // Expects Update to be notified about the change to active.
    // auto quit = loop.QuitClosure();
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          // Ends the test.
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, UnlockingScreen) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user unlocking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreen) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking the screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to unlocked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreenThenIdle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user locking screen.
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    // Simulates a user going idle, whilte the screen is still locked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to active.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          // Ends the test.
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(IdleManagerTest, LockingScreenAfterIdle) {
  mojo::Remote<blink::mojom::IdleManager> service_remote;

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    // Initial state of the system.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(0)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kActive, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates a user going idle, but with the screen still unlocked.
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(false));

    // Expects Update to be notified about the change to idle.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          EXPECT_EQ(blink::mojom::ScreenIdleState::kUnlocked, state->screen);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;
    // Simulates the screeng getting locked by the system after the user goes
    // idle (e.g. screensaver kicks in first, throwing idleness, then getting
    // locked).
    EXPECT_CALL(*mock, CalculateIdleTime())
        .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(60)));
    EXPECT_CALL(*mock, CheckIdleStateIsLocked())
        .WillRepeatedly(testing::Return(true));

    // Expects Update to be notified about the change to locked.
    EXPECT_CALL(monitor, Update(_))
        .WillOnce(Invoke([&](blink::mojom::IdleStatePtr state) {
          EXPECT_EQ(blink::mojom::ScreenIdleState::kLocked, state->screen);
          EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
          // Ends the test.
          loop.Quit();
        }));
    loop.Run();
  }
}

TEST_F(IdleManagerTest, RemoveMonitorStopsPolling) {
  // Simulates the renderer disconnecting (e.g. on page reload) and verifies
  // that the polling stops for the idle detection.

  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));

  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  {
    base::RunLoop loop;

    service_remote->AddMonitor(
        kThreshold, monitor_receiver.BindNewPipeAndPassRemote(),
        base::BindLambdaForTesting(
            [&](blink::mojom::IdleStatePtr state) { loop.Quit(); }));

    loop.Run();
  }

  EXPECT_TRUE(impl->IsPollingForTest());

  {
    base::RunLoop loop;

    // Simulates the renderer disconnecting.
    monitor_receiver.reset();

    // Wait for the IdleManager to observe the pipe close.
    loop.RunUntilIdle();
  }

  EXPECT_FALSE(impl->IsPollingForTest());
}

TEST_F(IdleManagerTest, Threshold) {
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  base::RunLoop loop;

  // Initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime())
      .WillRepeatedly(testing::Return(base::TimeDelta::FromSeconds(91)));
  EXPECT_CALL(*mock, CheckIdleStateIsLocked())
      .WillRepeatedly(testing::Return(false));

  service_remote->AddMonitor(
      base::TimeDelta::FromSeconds(90),
      monitor_receiver.BindNewPipeAndPassRemote(),
      base::BindLambdaForTesting([&](blink::mojom::IdleStatePtr state) {
        EXPECT_EQ(blink::mojom::UserIdleState::kIdle, state->user);
        loop.Quit();
      }));

  loop.Run();
}

TEST_F(IdleManagerTest, BadThreshold) {
  mojo::test::BadMessageObserver bad_message_observer;
  auto impl = std::make_unique<IdleManager>();
  auto* mock = new NiceMock<MockIdleTimeProvider>();
  impl->SetIdleTimeProviderForTest(base::WrapUnique(mock));
  mojo::Remote<blink::mojom::IdleManager> service_remote;
  impl->CreateService(service_remote.BindNewPipeAndPassReceiver());

  MockIdleMonitor monitor;
  mojo::Receiver<blink::mojom::IdleMonitor> monitor_receiver(&monitor);

  // Should not start initial state of the system.
  EXPECT_CALL(*mock, CalculateIdleTime()).Times(0);
  EXPECT_CALL(*mock, CheckIdleStateIsLocked()).Times(0);

  service_remote->AddMonitor(base::TimeDelta::FromSeconds(50),
                             monitor_receiver.BindNewPipeAndPassRemote(),
                             base::NullCallback());
  EXPECT_EQ("Minimum threshold is 60 seconds.",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace content
