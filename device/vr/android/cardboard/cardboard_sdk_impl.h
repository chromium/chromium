// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_

#include "device/vr/android/cardboard/cardboard_sdk.h"

namespace device {

class CardboardSdkImpl : public CardboardSdk {
 public:
  CardboardSdkImpl();
  ~CardboardSdkImpl() override;

  CardboardSdkImpl(const CardboardSdkImpl&) = delete;
  CardboardSdkImpl& operator=(const CardboardSdkImpl&) = delete;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
