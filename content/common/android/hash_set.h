// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_HASH_SET_H_
#define CONTENT_COMMON_ANDROID_HASH_SET_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

namespace content {

void JNI_Java_HashSet_add(JNIEnv* env,
                          const base::android::JavaRef<jobject>& hash_set,
                          const base::android::JavaRef<jobject>& object);

void JNI_Java_HashSet_remove(JNIEnv* env,
                             const base::android::JavaRef<jobject>& hash_set,
                             const base::android::JavaRef<jobject>& object);

void JNI_Java_HashSet_clear(JNIEnv* env,
                            const base::android::JavaRef<jobject>& hash_set);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_HASH_SET_H_
