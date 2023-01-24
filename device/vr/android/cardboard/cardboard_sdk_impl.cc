// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_sdk_impl.h"

#include "base/android/jni_android.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

namespace device {

CardboardSdkImpl::CardboardSdkImpl() {
  // Per the documentation this will be a no-op because of the nullptr.
  // TODO(https://crbug.com/989117): Move this to the RequestSession flow. It's
  // included for the time being just to ensure that the library is at least
  // used.
  Cardboard_initializeAndroid(base::android::GetVM(), nullptr);
}

CardboardSdkImpl::~CardboardSdkImpl() = default;

}  // namespace device
