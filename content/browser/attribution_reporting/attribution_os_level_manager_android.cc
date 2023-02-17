// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_os_level_manager_android.h"

#include "content/public/android/content_jni_headers/AttributionOsLevelManager_jni.h"
#include "url/android/gurl_android.h"
#include "url/origin.h"

namespace content {

AttributionOsLevelManagerAndroid::AttributionOsLevelManagerAndroid() = default;

AttributionOsLevelManagerAndroid::~AttributionOsLevelManagerAndroid() {
  if (jobj_) {
    Java_AttributionOsLevelManager_nativeDestroyed(
        base::android::AttachCurrentThread(), jobj_);
  }
}

void AttributionOsLevelManagerAndroid::RegisterAttributionSource(
    const GURL& registration_url,
    const url::Origin& top_level_origin,
    bool is_debug_key_allowed) {
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!jobj_) {
    jobj_ = Java_AttributionOsLevelManager_Constructor(
        env, reinterpret_cast<intptr_t>(this));
  }

  Java_AttributionOsLevelManager_registerAttributionSource(
      env, jobj_, url::GURLAndroid::FromNativeGURL(env, registration_url),
      url::GURLAndroid::FromNativeGURL(env, top_level_origin.GetURL()),
      is_debug_key_allowed);
}

}  // namespace content
