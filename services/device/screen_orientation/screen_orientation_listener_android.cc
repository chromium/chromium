// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/screen_orientation/screen_orientation_listener_android.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/task/current_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/screen_orientation/screen_orientation_jni_headers/ScreenOrientationListener_jni.h"

namespace device {

// static
void ScreenOrientationListenerAndroid::Create(
    mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ScreenOrientationListenerAndroid()),
      std::move(receiver));
}

ScreenOrientationListenerAndroid::ScreenOrientationListenerAndroid() = default;

ScreenOrientationListenerAndroid::~ScreenOrientationListenerAndroid() {
  DCHECK(base::CurrentIOThread::IsSet());
}

void ScreenOrientationListenerAndroid::IsAutoRotateEnabledByUser(
    IsAutoRotateEnabledByUserCallback callback) {
  std::move(callback).Run(
      Java_ScreenOrientationListener_isAutoRotateEnabledByUser(
          base::android::AttachCurrentThread()));
}

}  // namespace device
