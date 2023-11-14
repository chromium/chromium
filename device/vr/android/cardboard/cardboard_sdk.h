// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_

#include <jni.h>

#include "base/component_export.h"
#include "device/vr/android/xr_activity_state_handler.h"

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

  // Initializes the Cardboard SDK (by setting the JavaVM and Android context).
  virtual void Initialize(jobject context);
  // Launches the QR code scanner activity and saves the obtained encoded device
  // parameters. Meant to be used when it is not required to set a callback
  // function.
  virtual void ScanQrCodeAndSaveDeviceParams();
  // Launches the QR code scanner activity and saves the obtained encoded device
  // parameters. Meant to be used when it is required to set a callback function
  // for when the device parameters have been saved.
  virtual void ScanQrCodeAndSaveDeviceParams(
      std::unique_ptr<XrActivityStateHandler> activity_state_handler,
      base::OnceClosure on_params_saved);

  CardboardSdk(const CardboardSdk&) = delete;
  CardboardSdk& operator=(const CardboardSdk&) = delete;
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_SDK_H_
