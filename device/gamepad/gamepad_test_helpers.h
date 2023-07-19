// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
#define DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/gamepad_service.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "device/gamepad/public/cpp/gamepads.h"

namespace device {

// Data fetcher that returns canned data for the gamepad provider.
class MockGamepadDataFetcher : public GamepadDataFetcher {
 public:
  // Initializes the fetcher with the given gamepad data, which will be
  // returned when the provider queries us.
  explicit MockGamepadDataFetcher(const Gamepads& test_data);

  MockGamepadDataFetcher(const MockGamepadDataFetcher&) = delete;
  MockGamepadDataFetcher& operator=(const MockGamepadDataFetcher&) = delete;

  ~MockGamepadDataFetcher() override;

  GamepadSource source() override;

  // GamepadDataFetcher.
  void GetGamepadData(bool devices_changed_hint) override;

  // Blocks the current thread until the GamepadProvider reads from this
  // fetcher on the background thread.
  void WaitForDataRead();

  // Blocks the current thread until the GamepadProvider reads from this
  // fetcher on the background thread and issued all callbacks related to the
  // read on the client's thread.
  void WaitForDataReadAndCallbacksIssued();

  // Updates the test data.
  void SetTestData(const Gamepads& new_data);

 private:
  base::Lock lock_;
  Gamepads test_data_;
  base::WaitableEvent read_data_;
};

// Base class for the other test helpers. This just sets up the system monitor.
class GamepadTestHelper {
 public:
  GamepadTestHelper();

  GamepadTestHelper(const GamepadTestHelper&) = delete;
  GamepadTestHelper& operator=(const GamepadTestHelper&) = delete;

  virtual ~GamepadTestHelper();

 private:
  // This must be constructed before the system monitor.
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Constructs a GamepadService with a mock data source. This bypasses the
// global singleton for the gamepad service.
class GamepadServiceTestConstructor : public GamepadTestHelper {
 public:
  explicit GamepadServiceTestConstructor(const Gamepads& test_data);

  GamepadServiceTestConstructor(const GamepadServiceTestConstructor&) = delete;
  GamepadServiceTestConstructor& operator=(
      const GamepadServiceTestConstructor&) = delete;

  ~GamepadServiceTestConstructor() override;

  GamepadService* gamepad_service() { return gamepad_service_; }
  MockGamepadDataFetcher* data_fetcher() { return data_fetcher_; }

 private:
  // Owning pointer (can't be a scoped_ptr due to private destructor).
  raw_ptr<GamepadService, AcrossTasksDanglingUntriaged> gamepad_service_;

  // Pointer owned by the provider (which is owned by the gamepad service).
  raw_ptr<MockGamepadDataFetcher, AcrossTasksDanglingUntriaged> data_fetcher_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_TEST_HELPERS_H_
