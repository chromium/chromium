// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_WINRT_WGI_ENVIRONMENT_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_WINRT_WGI_ENVIRONMENT_H_

#include <hstring.h>

#include "base/win/windows_types.h"
#include "device/gamepad/test_support/wgi_test_error_code.h"

namespace device {

// Overrides the WinRT WGI OS APIs used by the helper functions in
// device/gamepad/wgi_data_fetcher_win.h. Also, it is used by the fake classes
// in tests to simulate failures by returning error HRESULT codes from fake
// WinRT API calls.
class FakeWinrtWgiEnvironment final {
 public:
  static HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                            const IID& iid,
                                            void** out_factory);

  FakeWinrtWgiEnvironment(WgiTestErrorCode error_code);
  FakeWinrtWgiEnvironment(const FakeWinrtWgiEnvironment&) = delete;
  FakeWinrtWgiEnvironment& operator=(const FakeWinrtWgiEnvironment&) = delete;
  ~FakeWinrtWgiEnvironment();

  // Injects errors in the fake implementation of the WinRT WGI APIs.
  void SimulateError(WgiTestErrorCode error_code);

  // Used by the fake WinRT WGI APIs to determine when to generate errors.
  static WgiTestErrorCode GetError();

 private:
  // The errors the fake WinRT WGI APIs should simulate. Set to |kOk| to succeed
  // without error.
  static WgiTestErrorCode s_error_code_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_WINRT_WGI_ENVIRONMENT_H_
