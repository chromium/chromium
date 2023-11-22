// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_iraw_game_controller.h"

#include "base/notreached.h"
#include "base/win/scoped_hstring.h"
#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"

namespace device {

FakeIRawGameController::FakeIRawGameController(int64_t gamepad_id,
                                               UINT16 hardware_product_id,
                                               UINT16 hardware_vendor_id,
                                               std::string_view display_name)
    : gamepad_id_(gamepad_id),
      hardware_product_id_(hardware_product_id),
      hardware_vendor_id_(hardware_vendor_id),
      display_name_(display_name) {}

FakeIRawGameController::~FakeIRawGameController() = default;

HRESULT WINAPI FakeIRawGameController::get_AxisCount(INT32* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::get_ButtonCount(INT32* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}
HRESULT WINAPI FakeIRawGameController::get_ForceFeedbackMotors(
    ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Gaming::Input::ForceFeedback::ForceFeedbackMotor*>**
        value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::get_HardwareProductId(UINT16* value) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiRawGameControllerGetHardwareProductIdFailed) {
    return E_FAIL;
  }
  *value = hardware_product_id_;
  return S_OK;
}

HRESULT WINAPI FakeIRawGameController::get_HardwareVendorId(UINT16* value) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiRawGameControllerGetHardwareVendorIdFailed) {
    return E_FAIL;
  }
  *value = hardware_vendor_id_;
  return S_OK;
}

HRESULT WINAPI FakeIRawGameController::get_SwitchCount(INT32* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::GetButtonLabel(
    INT32 buttonIndex,
    ABI::Windows::Gaming::Input::GameControllerButtonLabel* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::GetCurrentReading(
    UINT32 buttonArrayLength,
    boolean* buttonArray,
    UINT32 switchArrayLength,
    ABI::Windows::Gaming::Input::GameControllerSwitchPosition* switchArray,
    UINT32 axisArrayLength,
    DOUBLE* axisArray,
    UINT64* timestamp) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::GetSwitchKind(
    INT32 switchIndex,
    ABI::Windows::Gaming::Input::GameControllerSwitchKind* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::get_SimpleHapticsControllers(
    ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Devices::Haptics::SimpleHapticsController*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::get_NonRoamableId(HSTRING* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT WINAPI FakeIRawGameController::get_DisplayName(HSTRING* value) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiRawGameControllerGetDisplayNameFailed) {
    return E_FAIL;
  }
  *value = base::win::ScopedHString::Create(display_name_).release();
  return S_OK;
}

}  // namespace device
