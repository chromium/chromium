// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/jar_jni/HashSet_jni.h"

namespace content {

void JNI_Java_HashSet_add(JNIEnv* env,
                          const jni_zero::JavaRef<jobject>& hash_set,
                          const jni_zero::JavaRef<jobject>& object) {
  JNI_HashSet::Java_HashSet_add(env, hash_set, object);
}

void JNI_Java_HashSet_remove(JNIEnv* env,
                             const jni_zero::JavaRef<jobject>& hash_set,
                             const jni_zero::JavaRef<jobject>& object) {
  JNI_HashSet::Java_HashSet_remove(env, hash_set, object);
}

void JNI_Java_HashSet_clear(JNIEnv* env,
                            const jni_zero::JavaRef<jobject>& hash_set) {
  JNI_HashSet::Java_HashSet_clear(env, hash_set);
}

}  // namespace content
