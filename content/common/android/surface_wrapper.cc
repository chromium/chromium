// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/surface_wrapper.h"

#include "base/check.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/common_jni/SurfaceWrapper_jni.h"

namespace content {

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl,
    const base::android::JavaRef<jobject>& browserInputToken) {
  return Java_SurfaceWrapper_create(env, surface, canBeUsedWithSurfaceControl,
                                    browserInputToken);
}

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_create(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& surface,
    jboolean canBeUsedWithSurfaceControl) {
  return Java_SurfaceWrapper_create(env, surface, canBeUsedWithSurfaceControl);
}

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_createFromSurfaceControl(
    JNIEnv* env,
    gl::ScopedJavaSurfaceControl surface_control) {
  bool release_on_destroy = false;
  auto java_surface_control =
      surface_control.TakeJavaSurfaceControl(release_on_destroy);
  CHECK(!release_on_destroy);
  return Java_SurfaceWrapper_createFromSurfaceControl(env,
                                                      java_surface_control);
}

jboolean JNI_SurfaceWrapper_canBeUsedWithSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_canBeUsedWithSurfaceControl(env, obj);
}

jboolean JNI_SurfaceWrapper_getWrapsSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_getWrapsSurface(env, obj);
}

base::android::ScopedJavaLocalRef<jobject> JNI_SurfaceWrapper_takeSurface(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_takeSurface(env, obj);
}

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_takeSurfaceControl(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_takeSurfaceControl(env, obj);
}

base::android::ScopedJavaLocalRef<jobject>
JNI_SurfaceWrapper_getBrowserInputToken(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return Java_SurfaceWrapper_getBrowserInputToken(env, obj);
}

}  // namespace content.
