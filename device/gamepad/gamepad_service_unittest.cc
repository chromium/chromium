// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_service.h"

#include <string.h>

#include <memory>

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {
using ::base::test::RunClosure;

// The number of simulated gamepads that will be connected and disconnected in
// tests.
constexpr int kNumberOfGamepads = Gamepads::kItemsLengthCap;
}  // namespace

class MockGamepadConsumer : public GamepadConsumer {
 public:
  MockGamepadConsumer() {
    // Expect no connections or disconnections by default.
    EXPECT_CALL(*this, OnGamepadConnected).Times(0);
    EXPECT_CALL(*this, OnGamepadDisconnected).Times(0);
  }

  MockGamepadConsumer(MockGamepadConsumer&) = delete;
  MockGamepadConsumer& operator=(MockGamepadConsumer&) = delete;
  ~MockGamepadConsumer() override = default;

  MOCK_METHOD2(OnGamepadConnected, void(uint32_t, const Gamepad&));
  MOCK_METHOD2(OnGamepadDisconnected, void(uint32_t, const Gamepad&));
  MOCK_METHOD1(OnGamepadChanged, void(const mojom::GamepadChanges&));
};

class GamepadServiceTest : public testing::Test {
 public:
  GamepadServiceTest(const GamepadServiceTest&) = delete;
  GamepadServiceTest& operator=(const GamepadServiceTest&) = delete;

 protected:
  GamepadServiceTest() {
    memset(&test_data_, 0, sizeof(test_data_));

    // Configure the pad to have one button. We need our mock gamepad
    // to have at least one input so we can simulate a user gesture.
    test_data_.items[0].buttons_length = 1;
  }

  ~GamepadServiceTest() override = default;

  GamepadService* service() const { return service_; }

  void SetUp() override {
    auto fetcher = std::make_unique<MockGamepadDataFetcher>(test_data_);
    fetcher_ = fetcher.get();
    service_ = new GamepadService(std::move(fetcher));
    service_->SetSanitizationEnabled(false);
  }

  void TearDown() override {
    // Calling SetInstance will destroy the GamepadService instance.
    GamepadService::SetInstance(nullptr);
  }

  MockGamepadConsumer* CreateConsumer() {
    consumers_.push_back(std::make_unique<MockGamepadConsumer>());
    return consumers_.back().get();
  }

  // Configure the first `connected_count` gamepads as connected and the rest as
  // disconnected.
  void SetPadsConnected(int connected_count) {
    for (int i = 0; i < kNumberOfGamepads; ++i)
      test_data_.items[i].connected = (i < connected_count);
    fetcher_->SetTestData(test_data_);
  }

  void SimulateUserGesture(bool has_gesture) {
    test_data_.items[0].buttons[0].value = has_gesture ? 1.0f : 0.0f;
    test_data_.items[0].buttons[0].pressed = has_gesture ? true : false;
    fetcher_->SetTestData(test_data_);
  }

  void SimulatePageReload(GamepadConsumer* consumer) {
    EXPECT_TRUE(service_->ConsumerBecameInactive(consumer));
    EXPECT_TRUE(service_->ConsumerBecameActive(consumer));
  }

  void WaitForData() {
    // Block until work on the polling thread is complete. The data fetcher will
    // read gamepad data on the polling thread, which may cause the provider to
    // post user gesture or gamepad connection callbacks to the main thread.
    fetcher_->WaitForDataReadAndCallbacksIssued();

    // Allow the user gesture and gamepad connection callbacks to run.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<MockGamepadDataFetcher, AcrossTasksDanglingUntriaged> fetcher_;
  raw_ptr<GamepadService, AcrossTasksDanglingUntriaged> service_;
  std::vector<std::unique_ptr<MockGamepadConsumer>> consumers_;
  Gamepads test_data_;
};

TEST_F(GamepadServiceTest, ConnectionsTest) {
  // Create an active consumer.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));
  WaitForData();

  // Connect gamepads and simulate a user gesture. The consumer is notified for
  // each connected gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SimulateUserGesture(/*has_gesture=*/true);
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    loop.Run();
  }

  // Disconnect all gamepads. The consumer is notified for each disconnected
  // gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, ConnectionThenGestureTest) {
  // Create an active consumer.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));
  WaitForData();

  // Connect gamepads. The consumer is not notified because there is no gesture.
  SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
  WaitForData();

  // Simulate a user gesture. The consumer is notified for each connected
  // gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, ReloadTest) {
  // Create an active consumer.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));
  WaitForData();

  // Connect gamepads. The consumer is not notified because there is no gesture.
  SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
  WaitForData();

  // Simulate a page reload. The consumer is not notified because there is still
  // no gesture.
  SimulatePageReload(consumer);
  WaitForData();

  // Simulate a user gesture. The consumer is notified for each gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }

  // Simulate another page reload. The consumer is notified again for each
  // gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SimulatePageReload(consumer);
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, SecondConsumerGestureTest) {
  // Create two consumers and mark the first consumer active.
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  WaitForData();

  // Connect gamepads and simulate a user gesture. The active consumer is
  // notified, the inactive consumer is not.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }

  // Restore the default gamepad state (no gesture).
  SimulateUserGesture(/*has_gesture=*/false);

  // Mark the second consumer active. The second consumer is not notified
  // because it needs a new user gesture.
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();

  // Simulate another user gesture. Only the second consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, ConnectWhileInactiveTest) {
  // Create two consumers and mark them both active.
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Connect gamepads and simulate a user gesture. Each consumer is notified for
  // each connected gamepad.
  {
    base::RunLoop loop;
    auto barrier =
        base::BarrierClosure(2 * kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SimulateUserGesture(/*has_gesture=*/true);
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    loop.Run();
  }

  // Disconnect gamepads. Each consumer is notified for each disconnected
  // gamepad.
  {
    base::RunLoop loop;
    auto barrier =
        base::BarrierClosure(2 * kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }

  // Mark the second consumer inactive.
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();

  // Connect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    loop.Run();
  }

  // Mark the second consumer active again. The second consumer is notified
  // because it already received a gesture.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
    loop.Run();
  }
}

// https://crbug.com/1405460: Flaky on Android.
TEST_F(GamepadServiceTest, DISABLED_ConnectAndDisconnectWhileInactiveTest) {
  // Create two active consumers.
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Connect a gamepad and simulate a user gesture.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(2, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(1)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(1)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/1);
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }

  // Disconnect the gamepad.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(2, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadDisconnected)
        .Times(1)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadDisconnected)
        .Times(1)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }

  // Mark the second consumer inactive.
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();

  // Connect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    loop.Run();
  }

  // Disconnect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }

  // Mark the second consumer active again. The second consumer is not notified
  // because the connected gamepads were disconnected while the consumer was
  // still inactive.
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
}

// https://crbug.com/1346527 Flaky on Android and Linux.
TEST_F(GamepadServiceTest, DISABLED_DisconnectWhileInactiveTest) {
  // Create two active consumers.
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Connect gamepads and simulate a user gesture.
  {
    base::RunLoop loop;
    auto barrier =
        base::BarrierClosure(2 * kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }

  // Mark the second consumer inactive.
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();

  // Disconnect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }

  // Mark the second consumer active again. The second consumer is notified for
  // gamepads that were disconnected while it was inactive.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer2, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, DisconnectAndConnectWhileInactiveTest) {
  // Create two active consumers.
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Connect gamepads and simulate a user gesture.
  {
    base::RunLoop loop;
    auto barrier =
        base::BarrierClosure(2 * kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    SimulateUserGesture(/*has_gesture=*/true);
    loop.Run();
  }

  // Mark the second consumer inactive.
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();

  // Disconnect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadDisconnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/0);
    loop.Run();
  }

  // Connect gamepads. Only the active consumer is notified.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer1, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    SetPadsConnected(/*connected_count=*/kNumberOfGamepads);
    loop.Run();
  }

  // Mark the second consumer active again. The second consumer is notified for
  // gamepads that were connected while it was inactive.
  {
    base::RunLoop loop;
    auto barrier = base::BarrierClosure(kNumberOfGamepads, loop.QuitClosure());
    EXPECT_CALL(*consumer2, OnGamepadConnected)
        .Times(kNumberOfGamepads)
        .WillRepeatedly(RunClosure(barrier));
    EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
    loop.Run();
  }
}

TEST_F(GamepadServiceTest, ActiveConsumerBecameActive) {
  // Mark |consumer| active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Mark |consumer| active a second time. ConsumerBecameActive should fail.
  EXPECT_FALSE(service()->ConsumerBecameActive(consumer));
}

TEST_F(GamepadServiceTest, InactiveConsumerBecameInactive) {
  // Mark |consumer| active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Mark |consumer| inactive.
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer));

  // Mark |consumer| inactive a second time. ConsumerBecameInactive should fail.
  EXPECT_FALSE(service()->ConsumerBecameInactive(consumer));
}

TEST_F(GamepadServiceTest, UnregisteredConsumerBecameInactive) {
  auto* consumer = CreateConsumer();

  // |consumer| has not yet been added to the gamepad service through a call to
  // ConsumerBecameActive. ConsumerBecameInactive should fail.
  EXPECT_FALSE(service()->ConsumerBecameInactive(consumer));
}

TEST_F(GamepadServiceTest, RemoveUnregisteredConsumer) {
  auto* consumer = CreateConsumer();

  // |consumer| has not yet been added to the gamepad service through a call to
  // ConsumerBecameActive. RemoveConsumer should fail.
  EXPECT_FALSE(service()->RemoveConsumer(consumer));
}

}  // namespace device
