// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_service.h"

#include <string.h>

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {
constexpr int kNumberOfGamepads = Gamepads::kItemsLengthCap;
}  // namespace

class ConnectionListener : public GamepadConsumer {
 public:
  ConnectionListener() = default;

  // GamepadConsumer implementation.
  void OnGamepadConnected(uint32_t index, const Gamepad& gamepad) override {
    connected_counter_++;
  }
  void OnGamepadDisconnected(uint32_t index, const Gamepad& gamepad) override {
    disconnected_counter_++;
  }
  void OnGamepadButtonOrAxisChanged(uint32_t index,
                                    const Gamepad& gamepad) override {}

  void ClearCounters() {
    connected_counter_ = 0;
    disconnected_counter_ = 0;
  }

  int connected_counter() const { return connected_counter_; }
  int disconnected_counter() const { return disconnected_counter_; }

 private:
  int connected_counter_ = 0;
  int disconnected_counter_ = 0;
};

class GamepadServiceTest : public testing::Test {
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

  ConnectionListener* CreateConsumer() {
    consumers_.push_back(std::make_unique<ConnectionListener>());
    return consumers_.back().get();
  }

  void ClearCounters() {
    for (auto& consumer : consumers_)
      consumer->ClearCounters();
  }

  void SetPadsConnected(bool connected) {
    for (int i = 0; i < kNumberOfGamepads; ++i)
      test_data_.items[i].connected = connected;
    fetcher_->SetTestData(test_data_);
  }

  void SimulateUserGesture(bool has_gesture) {
    test_data_.items[0].buttons[0].value = has_gesture ? 1.f : 0.f;
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
  MockGamepadDataFetcher* fetcher_;
  GamepadService* service_;
  std::vector<std::unique_ptr<ConnectionListener>> consumers_;
  Gamepads test_data_;

  DISALLOW_COPY_AND_ASSIGN(GamepadServiceTest);
};

TEST_F(GamepadServiceTest, ConnectionsTest) {
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  SimulateUserGesture(true);
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());
}

TEST_F(GamepadServiceTest, ConnectionThenGestureTest) {
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  // No connection events are sent until a user gesture is seen.
  ClearCounters();
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  SimulateUserGesture(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());
}

TEST_F(GamepadServiceTest, ReloadTest) {
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // No connection events are sent until a user gesture is seen.
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  SimulatePageReload(consumer);
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  // After a user gesture, the connection listener is notified about connected
  // gamepads.
  ClearCounters();
  SimulateUserGesture(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  // After a reload, if the gamepads were already connected (and we have seen
  // a user gesture) then the connection listener is notified about connected
  // gamepads.
  ClearCounters();
  SimulatePageReload(consumer);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer->connected_counter());
  EXPECT_EQ(0, consumer->disconnected_counter());
}

TEST_F(GamepadServiceTest, SecondConsumerGestureTest) {
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));

  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  // Simulate a user gesture. The gesture is received before the second
  // consumer is active.
  ClearCounters();
  SetPadsConnected(true);
  SimulateUserGesture(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  // The second consumer becomes active, but should not receive connection
  // events until a new user gesture is received.
  ClearCounters();
  SimulateUserGesture(false);
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  // Connection events should only be sent to the second consumer.
  ClearCounters();
  SimulateUserGesture(true);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());
}

TEST_F(GamepadServiceTest, ConnectWhileInactiveTest) {
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Ensure the initial user gesture is received by both consumers.
  SimulateUserGesture(true);
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->disconnected_counter());

  // Check that connecting gamepads while a consumer is inactive will notify
  // once the consumer is active.
  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());
}

TEST_F(GamepadServiceTest, ConnectAndDisconnectWhileInactiveTest) {
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Ensure the initial user gesture is received by both consumers.
  SimulateUserGesture(true);
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->disconnected_counter());

  // Check that a connection and then disconnection is NOT reported once the
  // consumer is active.
  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());
}

TEST_F(GamepadServiceTest, DisconnectWhileInactiveTest) {
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Ensure the initial user gesture is received by both consumers.
  SimulateUserGesture(true);
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  // Check that disconnecting gamepads while a consumer is inactive will notify
  // once the consumer is active.
  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->disconnected_counter());

  ClearCounters();
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());
}

TEST_F(GamepadServiceTest, DisconnectAndConnectWhileInactiveTest) {
  auto* consumer1 = CreateConsumer();
  auto* consumer2 = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer1));
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));

  // Ensure the initial user gesture is received by both consumers.
  SimulateUserGesture(true);
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  // Check that a disconnection and then connection is reported as a connection
  // (and no disconnection) once the consumer is active.
  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameInactive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(false);
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  SetPadsConnected(true);
  WaitForData();
  EXPECT_EQ(kNumberOfGamepads, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(0, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());

  ClearCounters();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer2));
  WaitForData();
  EXPECT_EQ(0, consumer1->connected_counter());
  EXPECT_EQ(0, consumer1->disconnected_counter());
  EXPECT_EQ(kNumberOfGamepads, consumer2->connected_counter());
  EXPECT_EQ(0, consumer2->disconnected_counter());
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
