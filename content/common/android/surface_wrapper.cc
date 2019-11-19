// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_wrapper.h"

#include "content/public/android/content_jni_headers/SurfaceWrapper_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl) {
  return Java_SurfaceWrapper_create(env, surface, canBeUsedWithSurfaceControl);
}

jboolean JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_canBeUsedWithSurfaceControl(env, obj);
}

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_getSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_getSurface(env, obj);
}

}  // namespace content.
