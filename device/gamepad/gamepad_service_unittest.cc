// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_service.h"

#include <string.h>

#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/gamepad/gamepad_consumer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "device/gamepad/public/cpp/gamepad_features.h"
#include "device/gamepad/simulated_gamepad_data_fetcher.h"
#include "device/gamepad/simulated_gamepad_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {
using ::base::test::InvokeFuture;
using ::base::test::RunClosure;
using ::base::test::TestFuture;

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
    EXPECT_CALL(*this, OnGamepadRawInputChanged).Times(0);
  }

  MockGamepadConsumer(MockGamepadConsumer&) = delete;
  MockGamepadConsumer& operator=(MockGamepadConsumer&) = delete;
  ~MockGamepadConsumer() override = default;

  MOCK_METHOD2(OnGamepadConnected, void(uint32_t, const Gamepad&));
  MOCK_METHOD2(OnGamepadDisconnected, void(uint32_t, const Gamepad&));
  MOCK_METHOD2(OnGamepadRawInputChanged, void(uint32_t, const Gamepad&));
};

class GamepadServiceTest : public testing::Test {
 public:
  GamepadServiceTest(const GamepadServiceTest&) = delete;
  GamepadServiceTest& operator=(const GamepadServiceTest&) = delete;

 protected:
  GamepadServiceTest() {
    UNSAFE_TODO(memset(&test_data_, 0, sizeof(test_data_)));

    // Configure the pad to have one button. We need our mock gamepad
    // to have at least one input so we can simulate a user gesture.
    test_data_.items[0].buttons_length = 1;
  }

  ~GamepadServiceTest() override = default;

  GamepadService* service() const { return service_; }

  void SetUp() override {
    service_ = new GamepadService(CreateTestDataFetcher());
    service_->SetSanitizationEnabled(false);
  }

  virtual std::unique_ptr<GamepadDataFetcher> CreateTestDataFetcher() {
    auto fetcher = std::make_unique<MockGamepadDataFetcher>(test_data_);
    fetcher_ = fetcher.get();
    return fetcher;
  }

  void TearDown() override {
    fetcher_ = nullptr;
    service_->Terminate();
    service_ = nullptr;
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
  raw_ptr<MockGamepadDataFetcher> fetcher_ = nullptr;
  raw_ptr<GamepadService> service_ = nullptr;
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

class GamepadServiceSimulationTest : public GamepadServiceTest {
 public:
  std::unique_ptr<GamepadDataFetcher> CreateTestDataFetcher() override {
    auto fetcher = std::make_unique<SimulatedGamepadDataFetcher>();
    fetcher->SetOnPollForTesting(base::BindRepeating(
        &GamepadServiceSimulationTest::OnPoll, base::Unretained(this)));
    return fetcher;
  }

  void OnPoll() {
    if (poll_loop_.has_value()) {
      poll_loop_.value().Quit();
    }
  }

  void WaitForPoll() {
    poll_loop_.emplace();
    poll_loop_.value().Run();
    poll_loop_.reset();
  }

 protected:
  // Helper to create a gamepad with specified capabilities and establish user
  // gesture.
  base::UnguessableToken SetupRawInputGamepad(MockGamepadConsumer* consumer,
                                              SimulatedGamepadParams params) {
    params.name = "Raw input test gamepad";
    auto token = service()->AddSimulatedGamepad(std::move(params));

    // Establish user gesture with initial button press.
    TestFuture<uint32_t, const Gamepad&> connected_future;
    EXPECT_CALL(*consumer, OnGamepadConnected)
        .WillOnce(InvokeFuture(connected_future));
    service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                   /*pressed=*/std::nullopt,
                                   /*touched=*/std::nullopt);
    service()->SimulateInputFrame(token);

    EXPECT_EQ(connected_future.Get<0>(), 0u);
    EXPECT_TRUE(connected_future.Get<1>().connected);

    return token;
  }

  // Helper to clean up gamepad.
  void CleanupRawInputGamepad(MockGamepadConsumer* consumer,
                              base::UnguessableToken token) {
    TestFuture<uint32_t, const Gamepad&> disconnected_future;
    EXPECT_CALL(*consumer, OnGamepadDisconnected)
        .WillOnce(InvokeFuture(disconnected_future));
    service()->RemoveSimulatedGamepad(token);
    EXPECT_EQ(disconnected_future.Get<0>(), 0u);
    EXPECT_FALSE(disconnected_future.Get<1>().connected);
  }

 private:
  std::optional<base::RunLoop> poll_loop_;
};

TEST_F(GamepadServiceSimulationTest, ConnectDisconnect) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);
  EXPECT_TRUE(connected_future.Get<1>().connected);

  // Remove the simulated gamepad and check that OnGamepadDisconnected is
  // called.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  EXPECT_FALSE(disconnected_future.Get<1>().connected);
}

TEST_F(GamepadServiceSimulationTest, ConnectDisconnectMultiple) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params1;
  params1.name = "1 button";
  params1.button_bounds = {std::nullopt};
  auto token1 = service()->AddSimulatedGamepad(std::move(params1));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future1;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future1));
  service()->SimulateButtonInput(token1, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token1);
  EXPECT_EQ(connected_future1.Get<0>(), 0u);
  EXPECT_EQ(connected_future1.Get<1>().buttons_length, 1u);

  // Add a second simulated gamepad with two buttons.
  SimulatedGamepadParams params2;
  params2.name = "2 buttons";
  params2.button_bounds = {std::nullopt, std::nullopt};
  TestFuture<uint32_t, const Gamepad&> connected_future2;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future2));
  auto token2 = service()->AddSimulatedGamepad(std::move(params2));
  EXPECT_NE(token1, token2);
  EXPECT_EQ(connected_future2.Get<0>(), 1u);
  EXPECT_EQ(connected_future2.Get<1>().buttons_length, 2u);

  // Remove the simulated gamepads and check that `consumer` is notified each
  // time.
  TestFuture<uint32_t, const Gamepad&> disconnected_future1;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future1));
  service()->RemoveSimulatedGamepad(token1);
  EXPECT_EQ(disconnected_future1.Get<0>(), 0u);
  EXPECT_EQ(disconnected_future1.Get<1>().buttons_length, 1u);

  TestFuture<uint32_t, const Gamepad&> disconnected_future2;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future2));
  service()->RemoveSimulatedGamepad(token2);
  EXPECT_EQ(disconnected_future2.Get<0>(), 1u);
  EXPECT_EQ(disconnected_future2.Get<1>().buttons_length, 2u);
}

TEST_F(GamepadServiceSimulationTest, SimulateButtonInput) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {GamepadLogicalBounds(0.0, 255.0)};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/255.0,
                                 /*pressed=*/true, /*touched=*/true);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);
  const Gamepad& connected_gamepad = connected_future.Get<1>();
  EXPECT_EQ(connected_gamepad.buttons_length, 1u);
  EXPECT_EQ(connected_gamepad.buttons[0].value, 1.0);
  EXPECT_TRUE(connected_gamepad.buttons[0].pressed);
  EXPECT_TRUE(connected_gamepad.buttons[0].touched);

  // Release the button.
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.0,
                                 /*pressed=*/false, /*touched=*/false);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to
  // `consumer` in OnGamepadDisconnected shows the button is not pressed.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.buttons_length, 1u);
  EXPECT_EQ(disconnected_gamepad.buttons[0].value, 0.0);
  EXPECT_FALSE(disconnected_gamepad.buttons[0].pressed);
  EXPECT_FALSE(disconnected_gamepad.buttons[0].touched);
}

TEST_F(GamepadServiceSimulationTest, SimulateAxisInput) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one axis.
  SimulatedGamepadParams params;
  params.name = "1 axis 1 button";
  params.axis_bounds = {GamepadLogicalBounds(0.0, 255.0)};
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Move the axis.
  service()->SimulateAxisInput(token, /*index=*/0, /*logical_value=*/255.0);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to the
  // consumer in OnGamepadDisconnected shows the axis has moved.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.axes_length, 1u);
  EXPECT_EQ(disconnected_gamepad.axes[0], 1.0);
}

TEST_F(GamepadServiceSimulationTest, SimulateTouchInput) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one touch surface.
  SimulatedGamepadParams params;
  params.name = "1 button with touchpad";
  params.button_bounds = {std::nullopt};
  params.touch_surface_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Add a touch point and check that a touch ID is returned.
  auto touch_id =
      service()->SimulateTouchInput(token, /*surface_id=*/0, /*logical_x=*/0.0,
                                    /*logical_y=*/0.0);
  EXPECT_TRUE(touch_id.has_value());
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to the
  // consumer in OnGamepadDisconnected shows the touch point.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.touch_events_length, 1u);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].touch_id, touch_id.value());
  EXPECT_EQ(disconnected_gamepad.touch_events[0].surface_id, 0);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].x, 0.0);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].y, 0.0);
  EXPECT_FALSE(disconnected_gamepad.touch_events[0].has_surface_dimensions);
}

TEST_F(GamepadServiceSimulationTest, SimulateTouchMove) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one touch surface.
  SimulatedGamepadParams params;
  params.name = "1 button with touchpad";
  params.button_bounds = {std::nullopt};
  params.touch_surface_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Add a touch point and check that a touch ID is returned.
  auto touch_id =
      service()->SimulateTouchInput(token, /*surface_id=*/0, /*logical_x=*/0.0,
                                    /*logical_y=*/0.0);
  ASSERT_TRUE(touch_id.has_value());
  service()->SimulateInputFrame(token);

  // Move the touch point to a new location.
  service()->SimulateTouchMove(token, touch_id.value(), /*logical_x=*/1.0,
                               /*logical_y=*/1.0);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to the
  // consumer in OnGamepadDisconnected shows the updated touch point.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.touch_events_length, 1u);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].touch_id, touch_id.value());
  EXPECT_EQ(disconnected_gamepad.touch_events[0].x, 1.0);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].y, 1.0);
}

TEST_F(GamepadServiceSimulationTest, SimulateTouchEnd) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one touch surface.
  SimulatedGamepadParams params;
  params.name = "1 button with touchpad";
  params.button_bounds = {std::nullopt};
  params.touch_surface_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Add two touch points.
  auto touch_id0 =
      service()->SimulateTouchInput(token, /*surface_id=*/0, /*logical_x=*/0.0,
                                    /*logical_y=*/0.0);
  ASSERT_TRUE(touch_id0.has_value());
  auto touch_id1 =
      service()->SimulateTouchInput(token, /*surface_id=*/0, /*logical_x=*/0.0,
                                    /*logical_y=*/1.0);
  ASSERT_TRUE(touch_id1.has_value());
  service()->SimulateInputFrame(token);

  // Remove the first touch point.
  service()->SimulateTouchEnd(token, touch_id0.value());
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to the
  // consumer in OnGamepadDisconnected shows only the second touch point.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.touch_events_length, 1u);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].touch_id, touch_id1.value());
}

TEST_F(GamepadServiceSimulationTest, Vibration) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and a vibration actuator.
  SimulatedGamepadParams params;
  params.name = "1 button with vibration";
  params.button_bounds = {std::nullopt};
  params.vibration = {GamepadHapticEffectType::kDualRumble};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);
  EXPECT_TRUE(connected_future.Get<1>().vibration_actuator.not_null);
  EXPECT_EQ(connected_future.Get<1>().vibration_actuator.type,
            GamepadHapticActuatorType::kDualRumble);

  // Simulate vibration.
  TestFuture<mojom::GamepadHapticsResult> play_future;
  auto effect_params = mojom::GamepadEffectParameters::New();
  effect_params->strong_magnitude = 1.0;
  effect_params->weak_magnitude = 1.0;
  service()->PlayVibrationEffectOnce(
      /*pad_index=*/0,
      mojom::GamepadHapticEffectType::GamepadHapticEffectTypeDualRumble,
      std::move(effect_params), play_future.GetCallback());
  EXPECT_EQ(play_future.Get(),
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);

  // Simulate resetting vibration.
  TestFuture<mojom::GamepadHapticsResult> reset_future;
  service()->ResetVibrationActuator(/*pad_index=*/0,
                                    reset_future.GetCallback());
  EXPECT_EQ(reset_future.Get(),
            mojom::GamepadHapticsResult::GamepadHapticsResultComplete);

  // Remove the simulated gamepad.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
}

TEST_F(GamepadServiceSimulationTest, TokenNotFound) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Generate a random token and pass it to GamepadService methods that take a
  // token.
  EXPECT_CALL(*consumer, OnGamepadConnected).Times(0);
  EXPECT_CALL(*consumer, OnGamepadDisconnected).Times(0);
  auto random_token = base::UnguessableToken::Create();
  service()->RemoveSimulatedGamepad(random_token);
  service()->SimulateAxisInput(random_token, /*index=*/0,
                               /*logical_value=*/0.0);
  service()->SimulateButtonInput(
      random_token, /*index=*/0, /*logical_value*/ 0.0,
      /*pressed=*/std::nullopt, /*touched=*/std::nullopt);
  EXPECT_EQ(std::nullopt, service()->SimulateTouchInput(
                              random_token, /*surface_id=*/0, /*logical_x=*/0.0,
                              /*logical_y=*/0.0));
  service()->SimulateTouchMove(random_token, /*touch_id=*/0, /*logical_x=*/0.0,
                               /*logical_y=*/0.0);
  service()->SimulateTouchEnd(random_token, /*touch_id=*/0);
  service()->SimulateInputFrame(random_token);
}

// TODO(crbug.com/448993918): Re-enable this test
TEST_F(GamepadServiceSimulationTest, DISABLED_RemoveGamepadTwice) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Remove the simulated gamepad.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);

  // Remove the gamepad again and make sure `consumer` is not notified.
  EXPECT_CALL(*consumer, OnGamepadDisconnected).Times(0);
  service()->RemoveSimulatedGamepad(token);
}

TEST_F(GamepadServiceSimulationTest, InvalidButtonIndex) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {GamepadLogicalBounds(0.0, 255.0)};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/255.0,
                                 /*pressed=*/true, /*touched=*/true);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);
  const Gamepad& connected_gamepad = connected_future.Get<1>();
  EXPECT_EQ(connected_gamepad.buttons_length, 1u);
  EXPECT_EQ(connected_gamepad.buttons[0].value, 1.0);
  EXPECT_TRUE(connected_gamepad.buttons[0].pressed);
  EXPECT_TRUE(connected_gamepad.buttons[0].touched);

  // Simulate a button press for a button index that does not exist.
  service()->SimulateButtonInput(token, /*index=*/12345, /*logical_value=*/0.0,
                                 /*pressed=*/false, /*touched=*/false);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to
  // `consumer` in OnGamepadDisconnected shows the button is not pressed.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
}

TEST_F(GamepadServiceSimulationTest, InvalidAxisIndex) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one axis.
  SimulatedGamepadParams params;
  params.name = "1 axis 1 button";
  params.axis_bounds = {GamepadLogicalBounds(0.0, 255.0)};
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Simulate a axis input for an axis index that does not exist.
  service()->SimulateAxisInput(token, /*index=*/12345, /*logical_value=*/255.0);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.axes_length, 1u);
}

TEST_F(GamepadServiceSimulationTest, TouchIdNotFound) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button.
  SimulatedGamepadParams params;
  params.name = "1 button with touchpad";
  params.button_bounds = {std::nullopt};
  params.touch_surface_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Add a touch point.
  auto touch_id =
      service()->SimulateTouchInput(token, /*surface_id=*/0, /*logical_x=*/0.0,
                                    /*logical_y=*/0.0);
  ASSERT_TRUE(touch_id.has_value());
  service()->SimulateInputFrame(token);

  // Try moving a touch point with a non-existent touch ID.
  static constexpr uint32_t kFakeTouchId = 42;
  ASSERT_NE(touch_id.value(), kFakeTouchId);
  service()->SimulateTouchMove(token, kFakeTouchId, /*logical_x=*/1.0,
                               /*logical_y=*/1.0);
  service()->SimulateInputFrame(token);

  // Try ending a touch point with a non-existent touch ID.
  service()->SimulateTouchEnd(token, kFakeTouchId);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to
  // `consumer` in OnGamepadDisconnected has one touch point.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.touch_events_length, 1u);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].touch_id, touch_id.value());
  EXPECT_EQ(disconnected_gamepad.touch_events[0].x, 0.0);
  EXPECT_EQ(disconnected_gamepad.touch_events[0].y, 0.0);
}

TEST_F(GamepadServiceSimulationTest, SurfaceIdNotFound) {
  // Mark `consumer` active.
  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Add a simulated gamepad with one button and one touch surface.
  SimulatedGamepadParams params;
  params.name = "1 button with touchpad";
  params.button_bounds = {std::nullopt};
  params.touch_surface_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Simulate a button press and check that `consumer` is notified for the
  // connected gamepad.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));
  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  // Try adding a touch point with a non-existent surface ID.
  static constexpr uint32_t kFakeSurfaceId = 42;
  auto touch_id =
      service()->SimulateTouchInput(token, kFakeSurfaceId, /*logical_x=*/1.0,
                                    /*logical_y=*/1.0);
  ASSERT_FALSE(touch_id.has_value());

  WaitForPoll();

  // Remove the simulated gamepad and check that the Gamepad passed to
  // `consumer` in OnGamepadDisconnected has no touch points.
  TestFuture<uint32_t, const Gamepad&> disconnected_future;
  EXPECT_CALL(*consumer, OnGamepadDisconnected)
      .WillOnce(InvokeFuture(disconnected_future));
  service()->RemoveSimulatedGamepad(token);
  EXPECT_EQ(disconnected_future.Get<0>(), 0u);
  const Gamepad& disconnected_gamepad = disconnected_future.Get<1>();
  EXPECT_EQ(disconnected_gamepad.touch_events_length, 0u);
}

TEST_F(GamepadServiceSimulationTest, RawInputChangeDetectionButton) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  SimulatedGamepadParams params;
  params.name = "Raw input test gamepad";
  params.button_bounds = {std::nullopt};
  auto token = SetupRawInputGamepad(consumer, std::move(params));

  TestFuture<uint32_t, const Gamepad&> future;
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
      .WillOnce(InvokeFuture(future));

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.5,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  const auto [id, gamepad] = future.Take();
  EXPECT_EQ(gamepad.buttons_length, 1u);
  EXPECT_EQ(gamepad.buttons[0].value, 0.5);

  CleanupRawInputGamepad(consumer, token);
}

TEST_F(GamepadServiceSimulationTest, RawInputChangeDetectionAxis) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Set up a simulated gamepad with axis.
  SimulatedGamepadParams params;
  params.name = "Raw input test gamepad";
  params.button_bounds = {std::nullopt};
  params.axis_bounds = {GamepadLogicalBounds(-1.0, 1.0)};
  params.touch_surface_bounds = {std::nullopt};
  auto token = SetupRawInputGamepad(consumer, std::move(params));

  TestFuture<uint32_t, const Gamepad&> future;
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
      .WillOnce(InvokeFuture(future));

  service()->SimulateAxisInput(token, /*index=*/0, /*logical_value=*/0.5);
  service()->SimulateInputFrame(token);

  const auto [id, gamepad] = future.Take();
  EXPECT_EQ(gamepad.axes_length, 1u);
  EXPECT_EQ(gamepad.axes[0], 0.5);

  CleanupRawInputGamepad(consumer, token);
}

TEST_F(GamepadServiceSimulationTest, RawInputChangeDetectionTouch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  SimulatedGamepadParams params;
  params.name = "Raw input test gamepad";
  params.button_bounds = {std::nullopt};
  params.axis_bounds = {};
  params.touch_surface_bounds = {std::nullopt};
  auto token = SetupRawInputGamepad(consumer, std::move(params));

  // Test touch add.
  std::optional<uint32_t> touch_id;
  {
    TestFuture<uint32_t, const Gamepad&> future;
    EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
        .WillOnce(InvokeFuture(future));

    touch_id = service()->SimulateTouchInput(token, /*surface_id=*/0,
                                             /*logical_x=*/0.3,
                                             /*logical_y=*/0.7);
    ASSERT_TRUE(touch_id.has_value());
    service()->SimulateInputFrame(token);

    const auto [id, gamepad] = future.Take();
    EXPECT_EQ(gamepad.touch_events_length, 1u);
    EXPECT_EQ(gamepad.touch_events[0].touch_id, touch_id.value());
    EXPECT_FLOAT_EQ(gamepad.touch_events[0].x, 0.3f);
    EXPECT_FLOAT_EQ(gamepad.touch_events[0].y, 0.7f);
  }
  WaitForPoll();

  // Test touch move.
  {
    TestFuture<uint32_t, const Gamepad&> future;
    EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
        .WillOnce(InvokeFuture(future));

    service()->SimulateTouchMove(token, touch_id.value(), /*logical_x=*/0.6,
                                 /*logical_y=*/0.4);
    service()->SimulateInputFrame(token);

    const auto [id, gamepad] = future.Take();
    EXPECT_EQ(gamepad.touch_events_length, 1u);
    EXPECT_EQ(gamepad.touch_events[0].touch_id, touch_id.value());
    EXPECT_FLOAT_EQ(gamepad.touch_events[0].x, 0.6f);
    EXPECT_FLOAT_EQ(gamepad.touch_events[0].y, 0.4f);
  }
  WaitForPoll();

  // Test touch end.
  {
    TestFuture<uint32_t, const Gamepad&> future;
    EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
        .WillOnce(InvokeFuture(future));

    service()->SimulateTouchEnd(token, touch_id.value());
    service()->SimulateInputFrame(token);

    const auto [id, gamepad] = future.Take();
    EXPECT_EQ(gamepad.touch_events_length, 0u);
  }
  WaitForPoll();

  CleanupRawInputGamepad(consumer, token);
}

TEST_F(GamepadServiceSimulationTest,
       RawInputChangeDetectionMultipleInputTypes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  SimulatedGamepadParams params;
  params.button_bounds = {std::nullopt, std::nullopt};
  params.axis_bounds = {GamepadLogicalBounds(-1.0, 1.0)};
  params.touch_surface_bounds = {std::nullopt};

  auto token = SetupRawInputGamepad(consumer, std::move(params));

  std::optional<uint32_t> touch_id;
  base::RunLoop loop;
  TestFuture<uint32_t, const Gamepad&> future;
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
      .WillOnce(InvokeFuture(future));

  // Change multiple inputs in the same frame.
  service()->SimulateButtonInput(token, /*index=*/1, /*logical_value=*/0.8,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateAxisInput(token, /*index=*/0, /*logical_value=*/0.5);
  touch_id = service()->SimulateTouchInput(token, /*surface_id=*/0,
                                           /*logical_x=*/0.2,
                                           /*logical_y=*/0.9);
  ASSERT_TRUE(touch_id.has_value());

  service()->SimulateInputFrame(token);

  const auto [id, gamepad] = future.Take();
  EXPECT_EQ(gamepad.buttons_length, 2u);
  EXPECT_EQ(gamepad.buttons[1].value, 0.8);
  EXPECT_EQ(gamepad.axes_length, 1u);
  EXPECT_EQ(gamepad.axes[0], 0.5);
  EXPECT_EQ(gamepad.touch_events_length, 1u);
  EXPECT_EQ(gamepad.touch_events[0].touch_id, touch_id.value());
  EXPECT_FLOAT_EQ(gamepad.touch_events[0].x, 0.2f);
  EXPECT_FLOAT_EQ(gamepad.touch_events[0].y, 0.9f);
  CleanupRawInputGamepad(consumer, token);
}

TEST_F(GamepadServiceSimulationTest, RawInputChangeRequiresUserGesture) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  // Create gamepad without establishing user gesture.
  SimulatedGamepadParams params;
  params.name = "1 button";
  params.button_bounds = {std::nullopt};
  auto token = service()->AddSimulatedGamepad(std::move(params));

  // Expect no callbacks without user gesture.
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged).Times(0);
  EXPECT_CALL(*consumer, OnGamepadConnected).Times(0);

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.5,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  // Establish user gesture.
  TestFuture<uint32_t, const Gamepad&> connected_future;
  EXPECT_CALL(*consumer, OnGamepadConnected)
      .WillOnce(InvokeFuture(connected_future));

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/1.0,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);
  EXPECT_EQ(connected_future.Get<0>(), 0u);

  WaitForPoll();

  TestFuture<uint32_t, const Gamepad&> input_future;
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged)
      .WillOnce(InvokeFuture(input_future));

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.7,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  const auto [id, gamepad] = input_future.Take();
  EXPECT_EQ(gamepad.buttons[0].value, 0.7);

  CleanupRawInputGamepad(consumer, token);
}

TEST_F(GamepadServiceSimulationTest, RawInputChangeDisabledByFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kGamepadRawInputChangeEvent);

  auto* consumer = CreateConsumer();
  EXPECT_TRUE(service()->ConsumerBecameActive(consumer));

  SimulatedGamepadParams params;
  params.name = "Raw input test gamepad";
  params.button_bounds = {std::nullopt};
  auto token = SetupRawInputGamepad(consumer, std::move(params));

  // Even with user gesture, raw input changes should not trigger.
  EXPECT_CALL(*consumer, OnGamepadRawInputChanged).Times(0);

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.5,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  service()->SimulateButtonInput(token, /*index=*/0, /*logical_value=*/0.8,
                                 /*pressed=*/std::nullopt,
                                 /*touched=*/std::nullopt);
  service()->SimulateInputFrame(token);

  WaitForPoll();

  CleanupRawInputGamepad(consumer, token);
}

}  // namespace device
