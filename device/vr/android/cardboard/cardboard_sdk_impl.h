// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_

#include "device/vr/android/cardboard/cardboard_sdk.h"

#include "base/component_export.h"

namespace device {

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardSdkImpl : public CardboardSdk {
 public:
  CardboardSdkImpl();
  ~CardboardSdkImpl() override;

  void Initialize(jobject context) override;
  void ScanQrCodeAndSaveDeviceParams() override;

  CardboardSdkImpl(const CardboardSdkImpl&) = delete;
  CardboardSdkImpl& operator=(const CardboardSdkImpl&) = delete;

 private:
  bool initialized_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
