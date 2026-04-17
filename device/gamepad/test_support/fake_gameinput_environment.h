// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_GAMEINPUT_ENVIRONMENT_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_GAMEINPUT_ENVIRONMENT_H_

#include <GameInput.h>
#include <wrl.h>

#include "device/gamepad/test_support/fake_igameinput.h"

namespace device {

// Enum used in GameInputDataFetcher tests to simulate errors that might
// happen when interacting with the GameInput API.
enum class GameInputTestErrorCode {
  kOk,
  kGameInputCreateFailed,
  kGetProcAddressFailed,
  kDeviceCallbackRegistrationFailed,
  kGuideButtonCallbackRegistrationFailed,
  kNoDeviceInfo,
};

// Provides a test environment for the GameInput API used by
// GameInputDataFetcher. Encapsulates the creation of a FakeIGameInput
// instance and a fake GameInputCreate function that can be injected into the
// data fetcher for testing.
class FakeGameInputEnvironment final {
 public:
  // A fake replacement for the GameInputCreate function. Returns the
  // FakeIGameInput instance stored by the most recently constructed
  // FakeGameInputEnvironment.
  static HRESULT WINAPI GameInputCreate(IGameInput** game_input);

  explicit FakeGameInputEnvironment(
      GameInputTestErrorCode error_code = GameInputTestErrorCode::kOk);
  FakeGameInputEnvironment(const FakeGameInputEnvironment&) = delete;
  FakeGameInputEnvironment& operator=(const FakeGameInputEnvironment&) = delete;
  ~FakeGameInputEnvironment();

  // Injects errors in the fake implementation of the GameInput API.
  void SimulateError(GameInputTestErrorCode error_code);

  // Used by the fake GameInput API to determine when to generate errors.
  static GameInputTestErrorCode GetError();

  FakeIGameInput* GetFakeGameInput();

 private:
  static Microsoft::WRL::ComPtr<FakeIGameInput> fake_gameinput_;
  static GameInputTestErrorCode error_code_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_GAMEINPUT_ENVIRONMENT_H_
