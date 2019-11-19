// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/android/jar_jni/HashSet_jni.h"

namespace content {

void JNI_Java_HashSet_add(JNIEnv* env,
                          const base::android::JavaRef<jobject>& hash_set,
                          const base::android::JavaRef<jobject>& object) {
  JNI_HashSet::Java_HashSet_add(env, hash_set, object);
}

void JNI_Java_HashSet_remove(JNIEnv* env,
                             const base::android::JavaRef<jobject>& hash_set,
                             const base::android::JavaRef<jobject>& object) {
  JNI_HashSet::Java_HashSet_remove(env, hash_set, object);
}

void JNI_Java_HashSet_clear(JNIEnv* env,
                            const base::android::JavaRef<jobject>& hash_set) {
  JNI_HashSet::Java_HashSet_clear(env, hash_set);
}

}  // namespace content
