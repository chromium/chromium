// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_
#define CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_

#include "base/android/scoped_java_ref.h"
#include "ui/gl/android/scoped_java_surface_control.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl,
    const base::android::JavaRef<jobject>& browserInputToken);

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl);

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_createFromSurfaceControl(
    JNIEnv* env,
    gl::ScopedJavaSurfaceControl surface_control);

jboolean JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

jboolean JNI_SurfaceWrapper_getWrapsSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_takeSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_takeSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_getBrowserInputToken(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SURFACE_WRAPPER_H_
