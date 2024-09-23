// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/gamepad_provider.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "device/gamepad/public/cpp/gamepad_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// Helper class to generate and record user gesture callbacks.
class UserGestureListener {
 public:
  UserGestureListener() : has_user_gesture_(false) {}

  base::OnceClosure GetClosure() {
    return base::BindOnce(&UserGestureListener::GotUserGesture,
                          weak_factory_.GetWeakPtr());
  }

  bool has_user_gesture() const { return has_user_gesture_; }

 private:
  void GotUserGesture() { has_user_gesture_ = true; }

  bool has_user_gesture_;
  base::WeakPtrFactory<UserGestureListener> weak_factory_{this};
};

class TestChangeClient : public GamepadChangeClient {
 public:
  TestChangeClient() = default;
  TestChangeClient(const TestChangeClient&) = delete;
  TestChangeClient& operator=(const TestChangeClient&) = delete;
  ~TestChangeClient() = default;

  void OnGamepadConnectionChange(bool connected,
                                 uint32_t index,
                                 const Gamepad& pad) override {}

  void OnGamepadChange(mojom::GamepadChangesPtr changes) override {
    all_changes_.push_back(std::move(changes));
    EXPECT_GT(num_changes_left_, 0);
    if (--num_changes_left_ == 0)
      run_loop_.Quit();
  }

  void RunUntilChangeEvents(int num_changes) {
    // If we are explicitly not expecting any changes wait 20 milliseconds
    // to ensure no changes come in.
    if (num_changes == 0) {
      num_changes_left_ = 0;
      base::PlatformThread::Sleep(base::Milliseconds(20));
      base::RunLoop().RunUntilIdle();
      return;
    }
    num_changes_left_ = num_changes;
    run_loop_.Run();
  }

  const std::vector<mojom::GamepadChangesPtr>& all_changes() const {
    return all_changes_;
  }

 private:
  int num_changes_left_ = 0;
  base::RunLoop run_loop_;
  std::vector<mojom::GamepadChangesPtr> all_changes_;
};

// Main test fixture
class GamepadProviderTest : public testing::Test, public GamepadTestHelper {
 public:
  GamepadProviderTest(const GamepadProviderTest&) = delete;
  GamepadProviderTest& operator=(const GamepadProviderTest&) = delete;

  GamepadProvider* CreateProvider(const Gamepads& test_data) {
    auto fetcher = std::make_unique<MockGamepadDataFetcher>(test_data);
    mock_data_fetcher_ = fetcher.get();
    provider_ =
        std::make_unique<GamepadProvider>(&change_client_, std::move(fetcher),
                                          /*polling_thread=*/nullptr);
    return provider_.get();
  }

  // Sleep until the shared memory buffer's seqlock advances the buffer version,
  // indicating that the gamepad provider has written to it after polling the
  // gamepad fetchers. The buffer will report an odd value for the version if
  // the buffer is not in a consistent state, so we also require that the value
  // is even before continuing.
  void WaitForData(const GamepadHardwareBuffer* buffer) {
    const base::subtle::Atomic32 initial_version = buffer->seqlock.ReadBegin();
    base::subtle::Atomic32 current_version;
    do {
      base::PlatformThread::Sleep(base::Milliseconds(10));
      current_version = buffer->seqlock.ReadBegin();
    } while (current_version % 2 || current_version == initial_version);
  }

  // The provider polls the data on the background thread and then issues
  // the callback on the client thread. Waiting for it to poll twice ensures
  // that it was able to issue callbacks for the first poll.
  void WaitForDataAndCallbacksIssued(const GamepadHardwareBuffer* buffer) {
    WaitForData(buffer);
    WaitForData(buffer);
  }

  void ReadGamepadHardwareBuffer(const GamepadHardwareBuffer* buffer,
                                 Gamepads* output) {
    memset(output, 0, sizeof(Gamepads));
    base::subtle::Atomic32 version;
    do {
      version = buffer->seqlock.ReadBegin();
      memcpy(output, &buffer->data, sizeof(Gamepads));
    } while (buffer->seqlock.ReadRetry(version));
  }

 protected:
  GamepadProviderTest() = default;

  std::unique_ptr<GamepadProvider> provider_;

  // Pointer owned by the provider.
  raw_ptr<MockGamepadDataFetcher> mock_data_fetcher_;

  TestChangeClient change_client_;
};

TEST_F(GamepadProviderTest, PollingAccess) {
  Gamepads test_data;
  memset(&test_data, 0, sizeof(Gamepads));
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttons_length = 1;
  test_data.items[0].axes_length = 2;
  test_data.items[0].buttons[0].value = 1.0f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].axes[0] = -1.0f;
  test_data.items[0].axes[1] = 0.5f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();

  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(buffer, &output);

  ASSERT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(1.0f, output.items[0].buttons[0].value);
  EXPECT_EQ(true, output.items[0].buttons[0].pressed);
  ASSERT_EQ(2u, output.items[0].axes_length);
  EXPECT_EQ(-1.0f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
}

TEST_F(GamepadProviderTest, ConnectDisconnectMultiple) {
  Gamepads test_data;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes_length = 2;
  test_data.items[0].axes[0] = -1.0f;
  test_data.items[0].axes[1] = 0.5f;

  test_data.items[1].connected = true;
  test_data.items[1].timestamp = 0;
  test_data.items[1].axes_length = 2;
  test_data.items[1].axes[0] = 1.0f;
  test_data.items[1].axes[1] = -0.5f;

  Gamepads test_data_onedisconnected;
  test_data_onedisconnected.items[1].connected = true;
  test_data_onedisconnected.items[1].timestamp = 0;
  test_data_onedisconnected.items[1].axes_length = 2;
  test_data_onedisconnected.items[1].axes[0] = 1.0f;
  test_data_onedisconnected.items[1].axes[1] = -0.5f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();

  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(buffer, &output);

  ASSERT_EQ(2u, output.items[0].axes_length);
  EXPECT_EQ(-1.0f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
  ASSERT_EQ(2u, output.items[1].axes_length);
  EXPECT_EQ(1.0f, output.items[1].axes[0]);
  EXPECT_EQ(-0.5f, output.items[1].axes[1]);

  mock_data_fetcher_->SetTestData(test_data_onedisconnected);

  WaitForDataAndCallbacksIssued(buffer);

  ReadGamepadHardwareBuffer(buffer, &output);

  EXPECT_EQ(0u, output.items[0].axes_length);
  ASSERT_EQ(2u, output.items[1].axes_length);
  EXPECT_EQ(1.0f, output.items[1].axes[0]);
  EXPECT_EQ(-0.5f, output.items[1].axes[1]);
}

// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, UserGesture) {
  Gamepads no_button_data;
  no_button_data.items[0].connected = true;
  no_button_data.items[0].timestamp = 0;
  no_button_data.items[0].buttons_length = 1;
  no_button_data.items[0].axes_length = 2;
  no_button_data.items[0].buttons[0].value = 0.0f;
  no_button_data.items[0].buttons[0].pressed = false;
  no_button_data.items[0].axes[0] = 0.0f;
  no_button_data.items[0].axes[1] = 0.4f;

  Gamepads button_down_data = no_button_data;
  button_down_data.items[0].buttons[0].value = 1.0f;
  button_down_data.items[0].buttons[0].pressed = true;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(no_button_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();

  provider->RegisterForUserGesture(listener.GetClosure());

  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  // It should not have issued our callback.
  EXPECT_FALSE(listener.has_user_gesture());

  // Set a button down.
  mock_data_fetcher_->SetTestData(button_down_data);

  // The user gesture listener callback is not called until after the buffer has
  // been updated. Wait for the second update to ensure callbacks have fired.
  WaitForDataAndCallbacksIssued(buffer);

  // It should have issued our callback.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(listener.has_user_gesture());
}

// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, Sanitization) {
  Gamepads active_data;
  active_data.items[0].connected = true;
  active_data.items[0].timestamp = 0;
  active_data.items[0].buttons_length = 1;
  active_data.items[0].axes_length = 1;
  active_data.items[0].buttons[0].value = 1.0f;
  active_data.items[0].buttons[0].pressed = true;
  active_data.items[0].axes[0] = -1.0f;

  Gamepads zero_data;
  zero_data.items[0].connected = true;
  zero_data.items[0].timestamp = 0;
  zero_data.items[0].buttons_length = 1;
  zero_data.items[0].axes_length = 1;
  zero_data.items[0].buttons[0].value = 0.0f;
  zero_data.items[0].buttons[0].pressed = false;
  zero_data.items[0].axes[0] = 0.0f;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(active_data);
  provider->SetSanitizationEnabled(true);
  provider->Resume();

  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  Gamepads output;
  ReadGamepadHardwareBuffer(buffer, &output);

  // Initial data should all be zeroed out due to sanitization, even though the
  // gamepad reported input
  ASSERT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(0.0f, output.items[0].buttons[0].value);
  EXPECT_FALSE(output.items[0].buttons[0].pressed);
  ASSERT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(0.0f, output.items[0].axes[0]);

  // Zero out the inputs
  mock_data_fetcher_->SetTestData(zero_data);

  WaitForDataAndCallbacksIssued(buffer);

  // Read updated data from shared memory
  ReadGamepadHardwareBuffer(buffer, &output);

  // Should still read zero, which is now an accurate reflection of the data
  ASSERT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(0.0f, output.items[0].buttons[0].value);
  EXPECT_FALSE(output.items[0].buttons[0].pressed);
  ASSERT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(0.0f, output.items[0].axes[0]);

  // Re-set the active inputs
  mock_data_fetcher_->SetTestData(active_data);

  WaitForDataAndCallbacksIssued(buffer);

  // Read updated data from shared memory
  ReadGamepadHardwareBuffer(buffer, &output);

  // Should now accurately reflect the reported data.
  ASSERT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(1.0f, output.items[0].buttons[0].value);
  EXPECT_TRUE(output.items[0].buttons[0].pressed);
  ASSERT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(-1.0f, output.items[0].axes[0]);
}

TEST_F(GamepadProviderTest, SendEvents) {
  // This is a test for the logic that is currently behind this flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableGamepadButtonAxisEvents);

  Gamepads test_data;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes_length = 2;
  test_data.items[0].axes[0] = -1.0f;
  test_data.items[0].axes[1] = 0.5f;
  test_data.items[0].buttons[0].value = 0.0f;
  test_data.items[0].buttons[0].pressed = false;
  test_data.items[0].buttons[1].value = 0.7f;
  test_data.items[0].buttons[1].pressed = true;
  test_data.items[0].buttons[2].value = 0.0f;
  test_data.items[0].buttons[2].pressed = false;
  test_data.items[0].buttons[3].value = 0.0f;
  test_data.items[0].buttons[3].pressed = false;
  test_data.items[0].buttons_length = 4;
  test_data.items[0].axes_length = 2;

  test_data.items[1].connected = true;
  test_data.items[1].timestamp = 0;
  test_data.items[1].axes_length = 2;
  test_data.items[1].axes[0] = 1.0f;
  test_data.items[1].axes[1] = -0.5f;
  test_data.items[1].buttons[0].value = 0.0f;
  test_data.items[1].buttons[0].pressed = false;
  test_data.items[1].buttons[1].value = 1.0f;
  test_data.items[1].buttons[1].pressed = true;
  test_data.items[1].buttons[2].value = 1.0f;
  test_data.items[1].buttons[2].pressed = true;
  test_data.items[1].buttons_length = 3;
  test_data.items[1].axes_length = 2;

  Gamepads test_data_changed = test_data;
  test_data_changed.items[0].axes[1] = -0.5f;
  test_data_changed.items[0].buttons[0].value = 0.4f;
  test_data_changed.items[0].buttons[1].value = 0.2f;
  test_data_changed.items[0].buttons[1].pressed = false;
  test_data_changed.items[0].buttons[3].value = 0.2f;

  test_data_changed.items[1].axes[0] = 0.5f;
  test_data_changed.items[1].buttons[0].value = 1.0f;
  test_data_changed.items[1].buttons[0].pressed = true;
  test_data_changed.items[1].buttons[2].value = 0.9f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();
  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  mock_data_fetcher_->SetTestData(test_data_changed);

  // Wait for changes to take place and events to fire.
  WaitForDataAndCallbacksIssued(buffer);
  change_client_.RunUntilChangeEvents(2);

  const auto& changes = change_client_.all_changes();

  // Ensure the |button_changes| and |axis_changes| objects have all the
  // expected values.
  ASSERT_EQ(2u, changes.size());
  ASSERT_EQ(1u, changes[1]->axis_changes.size());
  ASSERT_EQ(1u, changes[0]->axis_changes.size());
  ASSERT_EQ(2u, changes[1]->button_changes.size());
  ASSERT_EQ(3u, changes[0]->button_changes.size());

  EXPECT_EQ(1u, changes[0]->axis_changes[0]->axis_index);
  EXPECT_EQ(-0.5f, changes[0]->axis_changes[0]->axis_snapshot);

  EXPECT_EQ(0u, changes[0]->button_changes[0]->button_index);
  EXPECT_FALSE(changes[0]->button_changes[0]->button_up);
  EXPECT_FALSE(changes[0]->button_changes[0]->button_down);
  EXPECT_TRUE(changes[0]->button_changes[0]->value_changed);
  EXPECT_EQ(changes[0]->button_changes[0]->button_snapshot,
            test_data_changed.items[0].buttons[0]);

  EXPECT_EQ(1u, changes[0]->button_changes[1]->button_index);
  EXPECT_TRUE(changes[0]->button_changes[1]->button_up);
  EXPECT_FALSE(changes[0]->button_changes[1]->button_down);
  EXPECT_TRUE(changes[0]->button_changes[1]->value_changed);
  EXPECT_EQ(changes[0]->button_changes[1]->button_snapshot,
            test_data_changed.items[0].buttons[1]);

  EXPECT_EQ(3u, changes[0]->button_changes[2]->button_index);
  EXPECT_FALSE(changes[0]->button_changes[2]->button_up);
  EXPECT_FALSE(changes[0]->button_changes[2]->button_down);
  EXPECT_TRUE(changes[0]->button_changes[2]->value_changed);
  EXPECT_EQ(changes[0]->button_changes[2]->button_snapshot,
            test_data_changed.items[0].buttons[3]);

  EXPECT_EQ(0u, changes[1]->axis_changes[0]->axis_index);
  EXPECT_EQ(0.5f, changes[1]->axis_changes[0]->axis_snapshot);

  EXPECT_EQ(0u, changes[1]->button_changes[0]->button_index);
  EXPECT_FALSE(changes[1]->button_changes[0]->button_up);
  EXPECT_TRUE(changes[1]->button_changes[0]->button_down);
  EXPECT_TRUE(changes[1]->button_changes[0]->value_changed);
  EXPECT_EQ(changes[1]->button_changes[0]->button_snapshot,
            test_data_changed.items[1].buttons[0]);

  EXPECT_EQ(2u, changes[1]->button_changes[1]->button_index);
  EXPECT_FALSE(changes[1]->button_changes[1]->button_up);
  EXPECT_FALSE(changes[1]->button_changes[1]->button_down);
  EXPECT_TRUE(changes[1]->button_changes[1]->value_changed);
  EXPECT_EQ(changes[1]->button_changes[1]->button_snapshot,
            test_data_changed.items[1].buttons[2]);
}

TEST_F(GamepadProviderTest, DontSendEventsBeforeUserGesture) {
  // This is a test for the logic that is currently behind this flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableGamepadButtonAxisEvents);

  Gamepads test_data;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes_length = 2;
  test_data.items[0].axes[0] = 0.0f;
  test_data.items[0].buttons[0].value = 0.0f;
  test_data.items[0].buttons[0].pressed = false;
  test_data.items[0].buttons_length = 1;
  test_data.items[0].axes_length = 1;

  Gamepads test_data_changed = test_data;
  test_data_changed.items[0].axes[1] = 0.4f;
  test_data_changed.items[0].buttons[0].value = 0.3f;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();
  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  mock_data_fetcher_->SetTestData(test_data_changed);

  // Wait for changes to take place and allow potential events to fire.
  WaitForDataAndCallbacksIssued(buffer);
  change_client_.RunUntilChangeEvents(0);

  EXPECT_TRUE(change_client_.all_changes().empty());
}

TEST_F(GamepadProviderTest, DontSendEventsWhenDisconnected) {
  // This is a test for the logic that is currently behind this flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableGamepadButtonAxisEvents);

  Gamepads test_data;
  test_data.items[0].connected = false;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes[0] = 0.0f;
  test_data.items[0].buttons[0].value = 1.0f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].buttons_length = 1;
  test_data.items[0].axes_length = 1;

  test_data.items[1].connected = true;
  test_data.items[1].timestamp = 0;
  test_data.items[1].axes[0] = 1.0f;
  test_data.items[1].buttons[0].value = 0.0f;
  test_data.items[1].buttons[0].pressed = false;
  test_data.items[1].buttons_length = 1;
  test_data.items[1].axes_length = 1;

  Gamepads test_data_changed = test_data;
  test_data_changed.items[0].axes[0] = 1.0f;
  test_data_changed.items[0].buttons[0].value = 0.0f;
  test_data_changed.items[0].buttons[0].pressed = false;

  test_data_changed.items[1].connected = false;
  test_data_changed.items[1].axes[0] = 0.0f;
  test_data_changed.items[1].buttons[0].value = 1.0f;
  test_data_changed.items[1].buttons[0].pressed = true;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();
  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  mock_data_fetcher_->SetTestData(test_data_changed);

  // Wait for changes to take place and allow potential events to fire.
  WaitForDataAndCallbacksIssued(buffer);
  change_client_.RunUntilChangeEvents(0);

  EXPECT_TRUE(change_client_.all_changes().empty());
}

TEST_F(GamepadProviderTest, DontSendEventsOnConnection) {
  // This is a test for the logic that is currently behind this flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableGamepadButtonAxisEvents);

  Gamepads test_data;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes[0] = 0.0f;
  test_data.items[0].buttons[0].value = 1.0f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].buttons_length = 1;
  test_data.items[0].axes_length = 1;

  test_data.items[1].connected = false;
  test_data.items[1].timestamp = 0;
  test_data.items[1].axes[0] = 0.0f;
  test_data.items[1].buttons[0].value = 1.0f;
  test_data.items[1].buttons[0].pressed = true;
  test_data.items[1].buttons_length = 1;
  test_data.items[1].axes_length = 1;

  Gamepads test_data_changed = test_data;
  test_data_changed.items[1].connected = true;
  test_data_changed.items[1].axes[0] = 1.0f;
  test_data_changed.items[1].buttons[0].value = 0.0f;
  test_data_changed.items[1].buttons[0].pressed = false;

  GamepadProvider* provider = CreateProvider(test_data);
  provider->SetSanitizationEnabled(false);
  provider->Resume();
  base::RunLoop().RunUntilIdle();

  // Renderer-side, pull data out of poll buffer.
  base::ReadOnlySharedMemoryRegion region =
      provider->DuplicateSharedMemoryRegion();
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  EXPECT_TRUE(mapping.IsValid());

  const GamepadHardwareBuffer* buffer =
      static_cast<const GamepadHardwareBuffer*>(mapping.memory());

  // Wait until the shared memory buffer has been written at least once.
  WaitForData(buffer);

  mock_data_fetcher_->SetTestData(test_data_changed);

  // Wait for changes to take place and allow potential events to fire.
  WaitForDataAndCallbacksIssued(buffer);
  change_client_.RunUntilChangeEvents(0);

  EXPECT_TRUE(change_client_.all_changes().empty());
}

}  // namespace

}  // namespace device
