// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "components/webxr/android/xr_jni_headers/CardboardUtils_jni.h"
#include "device/vr/android/cardboard/cardboard_device_params.h"

namespace webxr {

void JNI_CardboardUtils_NativeUseCardboardV1DeviceParamsForTesting(
    JNIEnv* env) {
  DVLOG(1) << __func__;
  device::CardboardDeviceParams::set_use_cardboard_v1_device_params_for_testing(
      true);
}

}  // namespace webxr
