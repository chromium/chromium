// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_ANDROID_H_
#define SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/haptics/haptics_manager_impl.h"
#include "services/device/public/mojom/haptics.mojom.h"

namespace device {

class HapticsManagerAndroid : public HapticsManagerImpl {
 public:
  static void Create(mojo::PendingReceiver<mojom::HapticsService> receiver);

  HapticsManagerAndroid();
  HapticsManagerAndroid(const HapticsManagerAndroid&) = delete;
  HapticsManagerAndroid& operator=(const HapticsManagerAndroid&) = delete;
  ~HapticsManagerAndroid() override;

 protected:
  // HapticsManagerImpl:
  void PlatformPlayHaptics(mojom::HapticEffect effect,
                           double intensity) override;

 private:
  const base::android::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HAPTICS_HAPTICS_MANAGER_ANDROID_H_
