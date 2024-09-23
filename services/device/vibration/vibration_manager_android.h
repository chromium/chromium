// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_ANDROID_H_
#define SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "services/device/vibration/vibration_manager_impl.h"

namespace device {

class VibrationManagerAndroid : public VibrationManagerImpl {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::VibrationManager> receiver,
      mojo::PendingRemote<mojom::VibrationManagerListener> listener);

  explicit VibrationManagerAndroid(
      mojo::PendingRemote<mojom::VibrationManagerListener> listener);
  VibrationManagerAndroid(const VibrationManagerAndroid&) = delete;
  VibrationManagerAndroid& operator=(const VibrationManagerAndroid&) = delete;
  ~VibrationManagerAndroid() override;

 protected:
  // VibrationManagerImpl:
  void PlatformVibrate(int64_t milliseconds) override;
  void PlatformCancel() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> impl_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_VIBRATION_VIBRATION_MANAGER_ANDROID_H_
