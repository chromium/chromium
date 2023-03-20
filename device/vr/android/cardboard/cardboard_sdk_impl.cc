// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_sdk_impl.h"

#include "base/android/jni_android.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

namespace device {

CardboardSdkImpl::CardboardSdkImpl() = default;

void CardboardSdkImpl::Initialize(jobject context) {
  Cardboard_initializeAndroid(base::android::GetVM(), context);
}

CardboardSdkImpl::~CardboardSdkImpl() = default;

}  // namespace device
