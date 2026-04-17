// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTDEVICE_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTDEVICE_H_

#include <GameInput.h>
#include <wrl.h>

namespace device {

class FakeIGameInputDevice final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IGameInputDevice> {
 public:
  FakeIGameInputDevice();
  FakeIGameInputDevice(uint16_t vendor, uint16_t product);
  FakeIGameInputDevice(uint16_t vendor,
                       uint16_t product,
                       GameInputRumbleMotors supported_rumble_motors);

  FakeIGameInputDevice(const FakeIGameInputDevice&) = delete;
  FakeIGameInputDevice& operator=(const FakeIGameInputDevice&) = delete;

  // IGameInputDevice fake implementation
  GameInputDeviceInfo const* STDMETHODCALLTYPE GetDeviceInfo() override;
  GameInputDeviceStatus STDMETHODCALLTYPE GetDeviceStatus() override;
  void STDMETHODCALLTYPE GetBatteryState(GameInputBatteryState*) override;
  HRESULT STDMETHODCALLTYPE
  CreateForceFeedbackEffect(uint32_t,
                            GameInputForceFeedbackParams const*,
                            IGameInputForceFeedbackEffect**) override;
  bool STDMETHODCALLTYPE IsForceFeedbackMotorPoweredOn(uint32_t) override;
  void STDMETHODCALLTYPE SetForceFeedbackMotorGain(uint32_t, float) override;
  HRESULT STDMETHODCALLTYPE
  SetHapticMotorState(uint32_t, GameInputHapticFeedbackParams const*) override;
  void STDMETHODCALLTYPE SetRumbleState(GameInputRumbleParams const*) override;
  void STDMETHODCALLTYPE SetInputSynchronizationState(bool) override;
  void STDMETHODCALLTYPE SendInputSynchronizationHint() override;
  void STDMETHODCALLTYPE PowerOff() override;
  HRESULT STDMETHODCALLTYPE
  CreateRawDeviceReport(uint32_t,
                        GameInputRawDeviceReportKind,
                        IGameInputRawDeviceReport**) override;
  HRESULT STDMETHODCALLTYPE
  GetRawDeviceFeature(uint32_t, IGameInputRawDeviceReport**) override;
  HRESULT STDMETHODCALLTYPE
  SetRawDeviceFeature(IGameInputRawDeviceReport*) override;
  HRESULT STDMETHODCALLTYPE
  SendRawDeviceOutput(IGameInputRawDeviceReport*) override;
  HRESULT STDMETHODCALLTYPE
  SendRawDeviceOutputWithResponse(IGameInputRawDeviceReport*,
                                  IGameInputRawDeviceReport**) override;
  HRESULT STDMETHODCALLTYPE ExecuteRawDeviceIoControl(uint32_t,
                                                      size_t,
                                                      void const*,
                                                      size_t,
                                                      void*,
                                                      size_t*) override;
  bool STDMETHODCALLTYPE AcquireExclusiveRawDeviceAccess(uint64_t) override;
  void STDMETHODCALLTYPE ReleaseExclusiveRawDeviceAccess() override;

 private:
  ~FakeIGameInputDevice() override;

  GameInputDeviceInfo deviceinfo_ = {};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUTDEVICE_H_
