// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IRAW_GAME_CONTROLLER_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IRAW_GAME_CONTROLLER_H_

#include <stdint.h>
#include <windows.foundation.collections.h>
#include <windows.gaming.input.h>
#include <wrl.h>

#include <string_view>

namespace device {

class FakeIRawGameController final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Gaming::Input::IRawGameController,
          ABI::Windows::Gaming::Input::IRawGameController2> {
 public:
  FakeIRawGameController(int64_t gamepad_id,
                         UINT16 hardware_product_id,
                         UINT16 hardware_vendor_id,
                         std::string_view display_name);

  FakeIRawGameController(const FakeIRawGameController&) = delete;
  FakeIRawGameController& operator=(const FakeIRawGameController&) = delete;

  // ABI::Windows::Gaming::Input::IRawGameController fake implementation.
  HRESULT WINAPI get_AxisCount(INT32* value) override;
  HRESULT WINAPI get_ButtonCount(INT32* value) override;
  HRESULT WINAPI get_ForceFeedbackMotors(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Gaming::Input::ForceFeedback::ForceFeedbackMotor*>**
          value) override;
  HRESULT WINAPI get_HardwareProductId(UINT16* value) override;
  HRESULT WINAPI get_HardwareVendorId(UINT16* value) override;
  HRESULT WINAPI get_SwitchCount(INT32* value) override;
  HRESULT WINAPI GetButtonLabel(
      INT32 buttonIndex,
      ABI::Windows::Gaming::Input::GameControllerButtonLabel* value) override;
  HRESULT WINAPI GetCurrentReading(
      UINT32 buttonArrayLength,
      boolean* buttonArray,
      UINT32 switchArrayLength,
      ABI::Windows::Gaming::Input::GameControllerSwitchPosition* switchArray,
      UINT32 axisArrayLength,
      DOUBLE* axisArray,
      UINT64* timestamp) override;
  HRESULT WINAPI GetSwitchKind(
      INT32 switchIndex,
      ABI::Windows::Gaming::Input::GameControllerSwitchKind* value) override;

  // ABI::Windows::Gaming::Input::IRawGameController2 fake implementation.
  HRESULT WINAPI get_SimpleHapticsControllers(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Haptics::SimpleHapticsController*>** value)
      override;
  HRESULT WINAPI get_NonRoamableId(HSTRING* value) override;
  HRESULT WINAPI get_DisplayName(HSTRING* value) override;

  uint64_t get_id() { return gamepad_id_; }
  void set_id(uint64_t gamepad_id) { gamepad_id_ = gamepad_id; }

 private:
  ~FakeIRawGameController() override;

  uint64_t gamepad_id_;
  UINT16 hardware_product_id_;
  UINT16 hardware_vendor_id_;
  std::string_view display_name_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IRAW_GAME_CONTROLLER_H_
