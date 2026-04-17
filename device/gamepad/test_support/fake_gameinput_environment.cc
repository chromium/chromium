// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_gameinput_environment.h"

namespace device {

// static
Microsoft::WRL::ComPtr<FakeIGameInput>
    FakeGameInputEnvironment::fake_gameinput_;

// static
GameInputTestErrorCode FakeGameInputEnvironment::error_code_ =
    GameInputTestErrorCode::kOk;

// static
HRESULT FakeGameInputEnvironment::GameInputCreate(IGameInput** game_input) {
  if (error_code_ == GameInputTestErrorCode::kGameInputCreateFailed ||
      !fake_gameinput_) {
    *game_input = nullptr;
    return E_FAIL;
  }
  return fake_gameinput_.CopyTo(game_input);
}

FakeGameInputEnvironment::FakeGameInputEnvironment(
    GameInputTestErrorCode error_code) {
  error_code_ = error_code;
  fake_gameinput_ = Microsoft::WRL::Make<FakeIGameInput>();
}

FakeGameInputEnvironment::~FakeGameInputEnvironment() {
  fake_gameinput_.Reset();
  error_code_ = GameInputTestErrorCode::kOk;
}

void FakeGameInputEnvironment::SimulateError(
    GameInputTestErrorCode error_code) {
  error_code_ = error_code;
}

// static
GameInputTestErrorCode FakeGameInputEnvironment::GetError() {
  return error_code_;
}

FakeIGameInput* FakeGameInputEnvironment::GetFakeGameInput() {
  return fake_gameinput_.Get();
}

}  // namespace device
