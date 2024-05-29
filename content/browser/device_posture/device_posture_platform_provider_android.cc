// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_posture/device_posture_platform_provider_android.h"

#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/DevicePosturePlatformProviderAndroid_jni.h"

namespace content {

using base::android::AttachCurrentThread;
using blink::mojom::DevicePostureType;

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

void DevicePosturePlatformProviderAndroid::NotifyDevicePostureChangeIfNeeded(
    bool is_folded) {
  DevicePostureType new_posture =
      is_folded ? DevicePostureType::kFolded : DevicePostureType::kContinuous;
  if (current_posture_ == new_posture) {
    return;
  }

  current_posture_ = new_posture;
  NotifyDevicePostureChanged(current_posture_);
}

void DevicePosturePlatformProviderAndroid::NotifyDisplayFeatureChangeIfNeeded(
    const gfx::Rect& display_feature_bounds) {
  if (current_display_feature_bounds_ == display_feature_bounds) {
    return;
  }

  current_display_feature_bounds_ = display_feature_bounds;
  NotifyDisplayFeatureBoundsChanged(current_display_feature_bounds_);
}

void DevicePosturePlatformProviderAndroid::UpdateDisplayFeature(
    JNIEnv* env,
    bool is_folded,
    int display_feature_bounds_left,
    int display_feature_bounds_top,
    int display_feature_bounds_right,
    int display_feature_bounds_bottom) {
  gfx::Rect display_feature;
  display_feature.SetByBounds(
      display_feature_bounds_left, display_feature_bounds_top,
      display_feature_bounds_right, display_feature_bounds_bottom);
  NotifyDisplayFeatureChangeIfNeeded(display_feature);
  NotifyDevicePostureChangeIfNeeded(is_folded);
}

}  // namespace content
