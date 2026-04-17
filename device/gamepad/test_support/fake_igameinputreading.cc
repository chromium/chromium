// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igameinputreading.h"

namespace device {

FakeIGameInputReading::FakeIGameInputReading() = default;
FakeIGameInputReading::~FakeIGameInputReading() = default;

FakeIGameInputReading::FakeIGameInputReading(const GameInputGamepadState state)
    : state_{state} {}

GameInputKind FakeIGameInputReading::GetInputKind() {
  return GameInputKindGamepad;
}

uint64_t FakeIGameInputReading::GetSequenceNumber(GameInputKind inputKind) {
  return 0;
}

uint64_t FakeIGameInputReading::GetTimestamp() {
  return 0;
}

void FakeIGameInputReading::GetDevice(IGameInputDevice** device) {
  *device = nullptr;
}

bool FakeIGameInputReading::GetRawReport(IGameInputRawDeviceReport** report) {
  return false;
}

uint32_t FakeIGameInputReading::GetControllerAxisCount() {
  return 0;
}

uint32_t FakeIGameInputReading::GetControllerAxisState(uint32_t stateArrayCount,
                                                       float* stateArray) {
  return 0;
}

uint32_t FakeIGameInputReading::GetControllerButtonCount() {
  return 0;
}

uint32_t FakeIGameInputReading::GetControllerButtonState(
    uint32_t stateArrayCount,
    bool* stateArray) {
  return 0;
}

uint32_t FakeIGameInputReading::GetControllerSwitchCount() {
  return 0;
}

uint32_t FakeIGameInputReading::GetControllerSwitchState(
    uint32_t stateArrayCount,
    GameInputSwitchPosition* stateArray) {
  return 0;
}

uint32_t FakeIGameInputReading::GetKeyCount() {
  return 0;
}

uint32_t FakeIGameInputReading::GetKeyState(uint32_t stateArrayCount,
                                            GameInputKeyState* stateArray) {
  return 0;
}

bool FakeIGameInputReading::GetMouseState(GameInputMouseState* state) {
  return false;
}

uint32_t FakeIGameInputReading::GetTouchCount() {
  return 0;
}

uint32_t FakeIGameInputReading::GetTouchState(uint32_t stateArrayCount,
                                              GameInputTouchState* stateArray) {
  return 0;
}

bool FakeIGameInputReading::GetMotionState(GameInputMotionState* state) {
  return false;
}

bool FakeIGameInputReading::GetArcadeStickState(
    GameInputArcadeStickState* state) {
  return false;
}

bool FakeIGameInputReading::GetFlightStickState(
    GameInputFlightStickState* state) {
  return false;
}

bool FakeIGameInputReading::GetGamepadState(GameInputGamepadState* state) {
  *state = state_;
  return true;
}

bool FakeIGameInputReading::GetRacingWheelState(
    GameInputRacingWheelState* state) {
  return false;
}

bool FakeIGameInputReading::GetUiNavigationState(
    GameInputUiNavigationState* state) {
  return false;
}

}  // namespace device
