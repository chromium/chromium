// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/vibration/vibration_manager_android.h"

#include "base/android/jni_android.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/vibration/android/vibration_jni_headers/VibrationManagerAndroid_jni.h"

namespace device {

VibrationManagerAndroid::VibrationManagerAndroid(
    mojo::PendingRemote<mojom::VibrationManagerListener> listener)
    : VibrationManagerImpl(std::move(listener)) {
  impl_.Reset(Java_VibrationManagerAndroid_getInstance(
      base::android::AttachCurrentThread()));
}
VibrationManagerAndroid::~VibrationManagerAndroid() = default;

void VibrationManagerAndroid::PlatformVibrate(int64_t milliseconds) {
  Java_VibrationManagerAndroid_vibrate(base::android::AttachCurrentThread(),
                                       impl_, milliseconds);
}

void VibrationManagerAndroid::PlatformCancel() {
  Java_VibrationManagerAndroid_cancel(base::android::AttachCurrentThread(),
                                      impl_);
}

void VibrationManagerAndroid::Create(
    mojo::PendingReceiver<mojom::VibrationManager> receiver,
    mojo::PendingRemote<mojom::VibrationManagerListener> listener) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VibrationManagerAndroid>(std::move(listener)),
      std::move(receiver));
}

}  // namespace device
