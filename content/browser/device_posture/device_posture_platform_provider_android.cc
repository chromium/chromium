// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_platform_provider_android.h"

#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/android/content_jni_headers/DevicePosturePlatformProviderAndroid_jni.h"

namespace content {

using base::android::AttachCurrentThread;

DevicePosturePlatformProviderAndroid::DevicePosturePlatformProviderAndroid(
    WebContents* web_contents) {
  DCHECK(web_contents);
  WebContentsAndroid* web_contents_android =
      static_cast<WebContentsImpl*>(web_contents)->GetWebContentsAndroid();
  java_device_posture_provider_.Reset(
      Java_DevicePosturePlatformProviderAndroid_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
          web_contents_android->GetJavaObject()));
}

DevicePosturePlatformProviderAndroid::~DevicePosturePlatformProviderAndroid() {
  Java_DevicePosturePlatformProviderAndroid_destroy(
      AttachCurrentThread(), java_device_posture_provider_);
}

void DevicePosturePlatformProviderAndroid::StartListening() {
  Java_DevicePosturePlatformProviderAndroid_startListening(
      AttachCurrentThread(), java_device_posture_provider_);
}

void DevicePosturePlatformProviderAndroid::StopListening() {
  Java_DevicePosturePlatformProviderAndroid_stopListening(
      AttachCurrentThread(), java_device_posture_provider_);
}

void DevicePosturePlatformProviderAndroid::SetDeviceFolded(JNIEnv* env,
                                                           bool is_folded) {
  blink::mojom::DevicePostureType new_posture =
      is_folded ? blink::mojom::DevicePostureType::kFolded
                : blink::mojom::DevicePostureType::kContinuous;
  if (current_posture_ == new_posture) {
    return;
  }

  current_posture_ = new_posture;
  NotifyDevicePostureChanged(current_posture_);
}

}  // namespace content
