// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_metadata.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "services/media_session/public/cpp/media_image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "services/media_session/public/cpp/android/media_session_jni_headers/MediaMetadata_jni.h"

using base::android::ScopedJavaLocalRef;

namespace media_session {

base::android::ScopedJavaLocalRef<jobject> MediaMetadata::CreateJavaObject(
    JNIEnv* env) const {
  ScopedJavaLocalRef<jstring> j_title(
      base::android::ConvertUTF16ToJavaString(env, title));
  ScopedJavaLocalRef<jstring> j_artist(
      base::android::ConvertUTF16ToJavaString(env, artist));
  ScopedJavaLocalRef<jstring> j_album(
      base::android::ConvertUTF16ToJavaString(env, album));
  return Java_MediaMetadata_create(env, j_title, j_artist, j_album);
}

}  // namespace media_session
