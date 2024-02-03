// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/browser/device_posture/device_posture_platform_provider.h"

namespace content {

class DevicePosturePlatformProviderAndroid
    : public DevicePosturePlatformProvider {
 public:
  explicit DevicePosturePlatformProviderAndroid(WebContents* web_contents);
  ~DevicePosturePlatformProviderAndroid() override;

  DevicePosturePlatformProviderAndroid(
      const DevicePosturePlatformProviderAndroid&) = delete;
  DevicePosturePlatformProviderAndroid& operator=(
      const DevicePosturePlatformProviderAndroid&) = delete;

  void StartListening() override;
  void StopListening() override;

  void SetDeviceFolded(JNIEnv* env, bool device_posture);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_device_posture_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_ANDROID_H_
