// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "services/service_manager/public/cpp/connector.h"
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

// Main test fixture
class GamepadProviderTest : public testing::Test, public GamepadTestHelper {
 public:
  GamepadProvider* CreateProvider(const Gamepads& test_data) {
    auto fetcher = std::make_unique<MockGamepadDataFetcher>(test_data);
    mock_data_fetcher_ = fetcher.get();
    provider_ = std::make_unique<GamepadProvider>(
        /*connection_change_client=*/nullptr,
        /*service_manager_connector=*/nullptr, std::move(fetcher),
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
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
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
  MockGamepadDataFetcher* mock_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(GamepadProviderTest);
};

TEST_F(GamepadProviderTest, PollingAccess) {
  Gamepads test_data;
  memset(&test_data, 0, sizeof(Gamepads));
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].buttons_length = 1;
  test_data.items[0].axes_length = 2;
  test_data.items[0].buttons[0].value = 1.f;
  test_data.items[0].buttons[0].pressed = true;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;

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

  EXPECT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(1.f, output.items[0].buttons[0].value);
  EXPECT_EQ(true, output.items[0].buttons[0].pressed);
  EXPECT_EQ(2u, output.items[0].axes_length);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
}

TEST_F(GamepadProviderTest, ConnectDisconnectMultiple) {
  Gamepads test_data;
  test_data.items[0].connected = true;
  test_data.items[0].timestamp = 0;
  test_data.items[0].axes_length = 2;
  test_data.items[0].axes[0] = -1.f;
  test_data.items[0].axes[1] = .5f;

  test_data.items[1].connected = true;
  test_data.items[1].timestamp = 0;
  test_data.items[1].axes_length = 2;
  test_data.items[1].axes[0] = 1.f;
  test_data.items[1].axes[1] = -.5f;

  Gamepads test_data_onedisconnected;
  test_data_onedisconnected.items[1].connected = true;
  test_data_onedisconnected.items[1].timestamp = 0;
  test_data_onedisconnected.items[1].axes_length = 2;
  test_data_onedisconnected.items[1].axes[0] = 1.f;
  test_data_onedisconnected.items[1].axes[1] = -.5f;

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

  EXPECT_EQ(2u, output.items[0].axes_length);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
  EXPECT_EQ(0.5f, output.items[0].axes[1]);
  EXPECT_EQ(2u, output.items[1].axes_length);
  EXPECT_EQ(1.f, output.items[1].axes[0]);
  EXPECT_EQ(-0.5f, output.items[1].axes[1]);

  mock_data_fetcher_->SetTestData(test_data_onedisconnected);

  WaitForDataAndCallbacksIssued(buffer);

  ReadGamepadHardwareBuffer(buffer, &output);

  EXPECT_EQ(0u, output.items[0].axes_length);
  EXPECT_EQ(2u, output.items[1].axes_length);
  EXPECT_EQ(1.f, output.items[1].axes[0]);
  EXPECT_EQ(-0.5f, output.items[1].axes[1]);
}

// Tests that waiting for a user gesture works properly.
TEST_F(GamepadProviderTest, UserGesture) {
  Gamepads no_button_data;
  no_button_data.items[0].connected = true;
  no_button_data.items[0].timestamp = 0;
  no_button_data.items[0].buttons_length = 1;
  no_button_data.items[0].axes_length = 2;
  no_button_data.items[0].buttons[0].value = 0.f;
  no_button_data.items[0].buttons[0].pressed = false;
  no_button_data.items[0].axes[0] = 0.f;
  no_button_data.items[0].axes[1] = .4f;

  Gamepads button_down_data = no_button_data;
  button_down_data.items[0].buttons[0].value = 1.f;
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
  active_data.items[0].buttons[0].value = 1.f;
  active_data.items[0].buttons[0].pressed = true;
  active_data.items[0].axes[0] = -1.f;

  Gamepads zero_data;
  zero_data.items[0].connected = true;
  zero_data.items[0].timestamp = 0;
  zero_data.items[0].buttons_length = 1;
  zero_data.items[0].axes_length = 1;
  zero_data.items[0].buttons[0].value = 0.f;
  zero_data.items[0].buttons[0].pressed = false;
  zero_data.items[0].axes[0] = 0.f;

  UserGestureListener listener;
  GamepadProvider* provider = CreateProvider(active_data);
  provider->SetSanitizationEnabled(true);
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

  // Initial data should all be zeroed out due to sanitization, even though the
  // gamepad reported input
  EXPECT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(0.f, output.items[0].buttons[0].value);
  EXPECT_FALSE(output.items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(0.f, output.items[0].axes[0]);

  // Zero out the inputs
  mock_data_fetcher_->SetTestData(zero_data);

  WaitForDataAndCallbacksIssued(buffer);

  // Read updated data from shared memory
  ReadGamepadHardwareBuffer(buffer, &output);

  // Should still read zero, which is now an accurate reflection of the data
  EXPECT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(0.f, output.items[0].buttons[0].value);
  EXPECT_FALSE(output.items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(0.f, output.items[0].axes[0]);

  // Re-set the active inputs
  mock_data_fetcher_->SetTestData(active_data);

  WaitForDataAndCallbacksIssued(buffer);

  // Read updated data from shared memory
  ReadGamepadHardwareBuffer(buffer, &output);

  // Should now accurately reflect the reported data.
  EXPECT_EQ(1u, output.items[0].buttons_length);
  EXPECT_EQ(1.f, output.items[0].buttons[0].value);
  EXPECT_TRUE(output.items[0].buttons[0].pressed);
  EXPECT_EQ(1u, output.items[0].axes_length);
  EXPECT_EQ(-1.f, output.items[0].axes[0]);
}

}  // namespace

}  // namespace device
