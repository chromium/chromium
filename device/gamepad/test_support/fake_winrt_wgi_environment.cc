// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"

#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/win/scoped_hstring.h"
#include "device/gamepad/test_support/fake_igamepad_statics.h"
#include "device/gamepad/wgi_data_fetcher_win.h"

namespace device {

// static
HRESULT FakeWinrtWgiEnvironment::FakeRoGetActivationFactory(
    HSTRING class_id,
    const IID& iid,
    void** out_factory) {
  base::win::ScopedHString class_id_hstring(class_id);
  HRESULT hr = S_OK;
  *out_factory = nullptr;

  Microsoft::WRL::ComPtr<FakeIGamepadStatics> fake_gamepad_statics =
      FakeIGamepadStatics::GetInstance();

  if (class_id_hstring.Get() == RuntimeClass_Windows_Gaming_Input_Gamepad) {
    if (FakeWinrtWgiEnvironment::GetError() ==
        WgiTestErrorCode::kErrorWgiGamepadActivateFailed) {
      hr = E_FAIL;
    } else {
      Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepadStatics>
          gamepad_statics;
      fake_gamepad_statics->QueryInterface(IID_PPV_ARGS(&gamepad_statics));
      *out_factory = gamepad_statics.Detach();
    }
  }

  else if (class_id_hstring.Get() ==
           RuntimeClass_Windows_Gaming_Input_RawGameController) {
    if (FakeWinrtWgiEnvironment::GetError() ==
        WgiTestErrorCode::kErrorWgiRawGameControllerActivateFailed) {
      hr = E_FAIL;
    } else {
      Microsoft::WRL::ComPtr<
          ABI::Windows::Gaming::Input::IRawGameControllerStatics>
          raw_game_controller_statics;
      fake_gamepad_statics->QueryInterface(
          IID_PPV_ARGS(&raw_game_controller_statics));
      *out_factory = raw_game_controller_statics.Detach();
    }
  }

  // Case an error happened, return the set HRESULT.
  if (hr != S_OK) {
    return hr;
  }

  if (*out_factory == nullptr) {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }
  return S_OK;
}

// static
WgiTestErrorCode FakeWinrtWgiEnvironment::s_error_code_ = WgiTestErrorCode::kOk;

FakeWinrtWgiEnvironment::FakeWinrtWgiEnvironment(WgiTestErrorCode error_code) {
  s_error_code_ = error_code;
  WgiDataFetcherWin::OverrideActivationFactoryFunctionForTesting(
      base::BindLambdaForTesting([]() {
        return &FakeWinrtWgiEnvironment::FakeRoGetActivationFactory;
      }));
}

FakeWinrtWgiEnvironment::~FakeWinrtWgiEnvironment() = default;

void FakeWinrtWgiEnvironment::SimulateError(WgiTestErrorCode error_code) {
  s_error_code_ = error_code;
}

// static
WgiTestErrorCode FakeWinrtWgiEnvironment::GetError() {
  return s_error_code_;
}

}  // namespace device
