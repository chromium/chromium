// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/mock_cardboard_sdk.h"

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "device/vr/android/xr_activity_state_handler.h"

using base::android::AttachCurrentThread;

namespace device {

// static
bool MockCardboardSdk::check_qr_code_scanner_was_launched_for_testing_ = false;

// static
bool MockCardboardSdk::check_qr_code_scanner_was_launched_for_testing() {
  return check_qr_code_scanner_was_launched_for_testing_;
}

MockCardboardSdk::MockCardboardSdk() = default;
MockCardboardSdk::~MockCardboardSdk() = default;

void MockCardboardSdk::Initialize(jobject context) {}

void MockCardboardSdk::ScanQrCodeAndSaveDeviceParams() {}

void MockCardboardSdk::ScanQrCodeAndSaveDeviceParams(
    std::unique_ptr<XrActivityStateHandler> activity_state_handler,
    base::OnceClosure on_params_saved) {
  check_qr_code_scanner_was_launched_for_testing_ = true;

  // Given that we are not launching the QR code activity on the mocked class,
  // we need to manually call the callback function so as to ensure the app
  // flows as expected.
  std::move(on_params_saved).Run();
}

}  // namespace device
