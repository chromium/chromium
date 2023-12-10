// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_

#include "device/vr/android/cardboard/cardboard_sdk.h"

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/android/xr_activity_state_handler.h"

namespace device {

class COMPONENT_EXPORT(VR_CARDBOARD) CardboardSdkImpl : public CardboardSdk {
 public:
  CardboardSdkImpl();
  ~CardboardSdkImpl() override;

  void Initialize(jobject context) override;
  void ScanQrCodeAndSaveDeviceParams() override;
  void ScanQrCodeAndSaveDeviceParams(
      std::unique_ptr<XrActivityStateHandler> activity_state_handler,
      base::OnceClosure on_params_saved) override;

  CardboardSdkImpl(const CardboardSdkImpl&) = delete;
  CardboardSdkImpl& operator=(const CardboardSdkImpl&) = delete;

 private:
  // We actually only care about being called back once, but the
  // XrActivityStateHandler requires a RepeatingClosure. Since a OnceClosure is
  // passed, this method serves as an abstraction to allow us to create a
  // RepeatingClosure.
  void OnActivityResumed();

  bool initialized_ = false;

  std::unique_ptr<XrActivityStateHandler> activity_state_handler_;
  base::OnceClosure on_params_saved_;

  base::WeakPtrFactory<CardboardSdkImpl> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_IMPL_H_
