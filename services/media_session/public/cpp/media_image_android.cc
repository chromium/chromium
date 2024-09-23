// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_image.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/media_session/public/cpp/android/media_session_jni_headers/MediaImage_jni.h"

using base::android::ScopedJavaLocalRef;

namespace media_session {

// static
ScopedJavaLocalRef<jobjectArray> MediaImage::ToJavaArray(
    JNIEnv* env,
    const std::vector<MediaImage>& images) {
  ScopedJavaLocalRef<jclass> string_clazz = base::android::GetClass(
      env, "org/chromium/services/media_session/MediaImage");
  jobjectArray joa =
      env->NewObjectArray(images.size(), string_clazz.obj(), NULL);
  base::android::CheckException(env);

  for (size_t i = 0; i < images.size(); ++i) {
    ScopedJavaLocalRef<jobject> item = images[i].CreateJavaObject(env);
    env->SetObjectArrayElement(joa, i, item.obj());
  }
  return ScopedJavaLocalRef<jobjectArray>(env, joa);
}

ScopedJavaLocalRef<jobject> MediaImage::CreateJavaObject(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> j_src(url::GURLAndroid::FromNativeGURL(env, src));
  ScopedJavaLocalRef<jstring> j_type(
      base::android::ConvertUTF16ToJavaString(env, type));

  // Create a Java array to store the sizes in.
  ScopedJavaLocalRef<jclass> string_clazz =
      base::android::GetClass(env, "android/graphics/Rect");
  jobjectArray joa =
      env->NewObjectArray(sizes.size(), string_clazz.obj(), NULL);
  base::android::CheckException(env);

  // Create an Android Rect for each size and store it in the array.
  for (size_t i = 0; i < sizes.size(); ++i) {
    ScopedJavaLocalRef<jobject> item =
        Java_MediaImage_createRect(env, sizes[i].width(), sizes[i].height());
    env->SetObjectArrayElement(joa, i, item.obj());
  }

  return Java_MediaImage_create(env, j_src, j_type,
                                ScopedJavaLocalRef<jobjectArray>(env, joa));
}

}  // namespace media_session
