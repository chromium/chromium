// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_
#define CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_

#include "base/android/scoped_java_ref.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl);

jboolean JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_getSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_
