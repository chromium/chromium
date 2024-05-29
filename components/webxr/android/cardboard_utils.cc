// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "components/webxr/android/cardboard_device_provider.h"
#include "device/vr/android/cardboard/cardboard_device_params.h"
#include "device/vr/android/cardboard/mock_cardboard_sdk.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webxr/android/xr_jni_headers/CardboardUtils_jni.h"

namespace webxr {

void JNI_CardboardUtils_NativeUseCardboardV1DeviceParamsForTesting(
    JNIEnv* env) {
  DVLOG(1) << __func__;
  device::CardboardDeviceParams::set_use_cardboard_v1_device_params_for_testing(
      true);
}

void JNI_CardboardUtils_NativeUseCardboardMockForTesting(JNIEnv* env) {
  DVLOG(1) << __func__;
  CardboardDeviceProvider::set_use_cardboard_mock_for_testing(true);
}

jboolean JNI_CardboardUtils_NativeCheckQrCodeScannerWasLaunchedForTesting(
    JNIEnv* env) {
  DVLOG(1) << __func__;
  return device::MockCardboardSdk::
      check_qr_code_scanner_was_launched_for_testing();
}

}  // namespace webxr
