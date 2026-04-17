// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igameinputdevice.h"

#include "device/gamepad/test_support/fake_gameinput_environment.h"

namespace device {

FakeIGameInputDevice::~FakeIGameInputDevice() = default;

FakeIGameInputDevice::FakeIGameInputDevice() {
  deviceinfo_.vendorId = 0x45e;
  deviceinfo_.productId = 0x2ea;
}

FakeIGameInputDevice::FakeIGameInputDevice(uint16_t vendor, uint16_t product) {
  deviceinfo_.vendorId = vendor;
  deviceinfo_.productId = product;
}

FakeIGameInputDevice::FakeIGameInputDevice(
    uint16_t vendor,
    uint16_t product,
    GameInputRumbleMotors supported_rumble_motors) {
  deviceinfo_.vendorId = vendor;
  deviceinfo_.productId = product;
  deviceinfo_.supportedRumbleMotors = supported_rumble_motors;
}

GameInputDeviceInfo const* FakeIGameInputDevice::GetDeviceInfo() {
  if (FakeGameInputEnvironment::GetError() ==
      GameInputTestErrorCode::kNoDeviceInfo) {
    return nullptr;
  }
  return &deviceinfo_;
}

GameInputDeviceStatus FakeIGameInputDevice::GetDeviceStatus() {
  return GameInputDeviceNoStatus;
}

void FakeIGameInputDevice::GetBatteryState(GameInputBatteryState*) {}

HRESULT FakeIGameInputDevice::CreateForceFeedbackEffect(
    uint32_t,
    GameInputForceFeedbackParams const*,
    IGameInputForceFeedbackEffect**) {
  return E_NOTIMPL;
}

bool FakeIGameInputDevice::IsForceFeedbackMotorPoweredOn(uint32_t) {
  return false;
}

void FakeIGameInputDevice::SetForceFeedbackMotorGain(uint32_t, float) {}

HRESULT FakeIGameInputDevice::SetHapticMotorState(
    uint32_t,
    GameInputHapticFeedbackParams const*) {
  return E_NOTIMPL;
}

void FakeIGameInputDevice::SetRumbleState(GameInputRumbleParams const*) {}

void FakeIGameInputDevice::SetInputSynchronizationState(bool) {}

void FakeIGameInputDevice::SendInputSynchronizationHint() {}

void FakeIGameInputDevice::PowerOff() {}

HRESULT FakeIGameInputDevice::CreateRawDeviceReport(
    uint32_t,
    GameInputRawDeviceReportKind,
    IGameInputRawDeviceReport**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInputDevice::GetRawDeviceFeature(uint32_t,
                                                  IGameInputRawDeviceReport**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInputDevice::SetRawDeviceFeature(IGameInputRawDeviceReport*) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInputDevice::SendRawDeviceOutput(IGameInputRawDeviceReport*) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInputDevice::SendRawDeviceOutputWithResponse(
    IGameInputRawDeviceReport*,
    IGameInputRawDeviceReport**) {
  return E_NOTIMPL;
}

HRESULT FakeIGameInputDevice::ExecuteRawDeviceIoControl(uint32_t,
                                                        size_t,
                                                        void const*,
                                                        size_t,
                                                        void*,
                                                        size_t*) {
  return E_NOTIMPL;
}

bool FakeIGameInputDevice::AcquireExclusiveRawDeviceAccess(uint64_t) {
  return false;
}

void FakeIGameInputDevice::ReleaseExclusiveRawDeviceAccess() {}

}  // namespace device
