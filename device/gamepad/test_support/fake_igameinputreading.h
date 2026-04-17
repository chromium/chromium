// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTREADING_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTREADING_H_

#include <GameInput.h>
#include <wrl.h>

namespace device {

class FakeIGameInputReading final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IGameInputReading> {
 public:
  FakeIGameInputReading();
  FakeIGameInputReading(const FakeIGameInputReading&) = delete;
  FakeIGameInputReading& operator=(const FakeIGameInputReading&) = delete;
  FakeIGameInputReading(const GameInputGamepadState state);

  // IGameInputReading implementation
  GameInputKind STDMETHODCALLTYPE GetInputKind() override;
  uint64_t STDMETHODCALLTYPE
  GetSequenceNumber(GameInputKind inputKind) override;
  uint64_t STDMETHODCALLTYPE GetTimestamp() override;
  void STDMETHODCALLTYPE GetDevice(IGameInputDevice** device) override;
  bool STDMETHODCALLTYPE
  GetRawReport(IGameInputRawDeviceReport** report) override;
  uint32_t STDMETHODCALLTYPE GetControllerAxisCount() override;
  uint32_t STDMETHODCALLTYPE GetControllerAxisState(uint32_t stateArrayCount,
                                                    float* stateArray) override;
  uint32_t STDMETHODCALLTYPE GetControllerButtonCount() override;
  uint32_t STDMETHODCALLTYPE
  GetControllerButtonState(uint32_t stateArrayCount, bool* stateArray) override;
  uint32_t STDMETHODCALLTYPE GetControllerSwitchCount() override;
  uint32_t STDMETHODCALLTYPE
  GetControllerSwitchState(uint32_t stateArrayCount,
                           GameInputSwitchPosition* stateArray) override;
  uint32_t STDMETHODCALLTYPE GetKeyCount() override;
  uint32_t STDMETHODCALLTYPE
  GetKeyState(uint32_t stateArrayCount, GameInputKeyState* stateArray) override;
  bool STDMETHODCALLTYPE GetMouseState(GameInputMouseState* state) override;
  uint32_t STDMETHODCALLTYPE GetTouchCount() override;
  uint32_t STDMETHODCALLTYPE
  GetTouchState(uint32_t stateArrayCount,
                GameInputTouchState* stateArray) override;
  bool STDMETHODCALLTYPE GetMotionState(GameInputMotionState* state) override;
  bool STDMETHODCALLTYPE
  GetArcadeStickState(GameInputArcadeStickState* state) override;
  bool STDMETHODCALLTYPE
  GetFlightStickState(GameInputFlightStickState* state) override;
  bool STDMETHODCALLTYPE GetGamepadState(GameInputGamepadState* state) override;
  bool STDMETHODCALLTYPE
  GetRacingWheelState(GameInputRacingWheelState* state) override;
  bool STDMETHODCALLTYPE
  GetUiNavigationState(GameInputUiNavigationState* state) override;

 private:
  ~FakeIGameInputReading() override;

  GameInputGamepadState state_ = {};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTREADING_H_
