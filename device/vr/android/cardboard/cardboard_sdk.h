// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_

#include <jni.h>

#include "base/component_export.h"

namespace device {

// The actual cardboard SDK provides a C-Style interface. This wrapper provides
// a bit more modern interface both to help abstract some of the create/destroy
// patterns that return/take raw pointers so that we can wrap them in a class
// that will automatically clean them up, as well as provides a mechanism to
// shim out the Cardboard SDK for testing purposes.
class COMPONENT_EXPORT(VR_CARDBOARD) CardboardSdk {
 public:
  CardboardSdk() = default;
  virtual ~CardboardSdk() = default;

  virtual void Initialize(jobject context);
  virtual void ScanQrCodeAndSaveDeviceParams();

  CardboardSdk(const CardboardSdk&) = delete;
  CardboardSdk& operator=(const CardboardSdk&) = delete;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_
