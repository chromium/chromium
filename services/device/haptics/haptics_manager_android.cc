// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/haptics/haptics_manager_android.h"

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/device/haptics/android/haptics_jni_headers/HapticsManagerAndroid_jni.h"

namespace device {

HapticsManagerAndroid::HapticsManagerAndroid()
    : impl_(Java_HapticsManagerAndroid_create(
          base::android::AttachCurrentThread())) {
  CHECK(impl_);
}

HapticsManagerAndroid::~HapticsManagerAndroid() = default;

void HapticsManagerAndroid::PlatformPlayHaptics(mojom::HapticEffect effect,
                                                double intensity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Java_HapticsManagerAndroid_playHaptics(base::android::AttachCurrentThread(),
                                         impl_, static_cast<int>(effect),
                                         intensity);
}

// static
void HapticsManagerAndroid::Create(
    mojo::PendingReceiver<mojom::HapticsService> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<HapticsManagerAndroid>(),
                              std::move(receiver));
}

}  // namespace device

DEFINE_JNI(HapticsManagerAndroid)
