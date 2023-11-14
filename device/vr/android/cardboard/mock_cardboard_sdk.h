// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_MOCK_CARDBOARD_SDK_H_
#define DEVICE_VR_ANDROID_CARDBOARD_MOCK_CARDBOARD_SDK_H_

#include "device/vr/android/cardboard/cardboard_sdk.h"

#include "base/component_export.h"
#include "device/vr/android/xr_activity_state_handler.h"

namespace device {

class COMPONENT_EXPORT(VR_CARDBOARD) MockCardboardSdk : public CardboardSdk {
 public:
  static bool check_qr_code_scanner_was_launched_for_testing();

  MockCardboardSdk();
  ~MockCardboardSdk() override;

  void Initialize(jobject context) override;
  void ScanQrCodeAndSaveDeviceParams() override;
  void ScanQrCodeAndSaveDeviceParams(
      std::unique_ptr<XrActivityStateHandler> activity_state_handler,
      base::OnceClosure on_params_saved) override;

  MockCardboardSdk(const MockCardboardSdk&) = delete;
  MockCardboardSdk& operator=(const MockCardboardSdk&) = delete;

 private:
  // This flag is true if
  // `MockCardboardSdk::ScanQrCodeAndSaveDeviceParams()` has been executed,
  // otherwise it is false. Meant to be used for testing purposes only.
  static bool check_qr_code_scanner_was_launched_for_testing_;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_MOCK_CARDBOARD_SDK_H_
