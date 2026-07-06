// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/child_process_id.h"

#include "content/public/android/content_main_dex_jni/ChildProcessId_jni.h"
#include "content/public/common/content_constants.h"

namespace jni_zero {

template <>
content::ChildProcessId FromJniType<content::ChildProcessId>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  if (!j_object) {
    return content::ChildProcessId();
  }
  return content::ChildProcessId(
      content::Java_ChildProcessId_getProcessIdForSerialization(env, j_object));
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<content::ChildProcessId>(
    JNIEnv* env,
    const content::ChildProcessId& process_id) {
  // Normalise invalid IDs to kInvalidChildProcessUniqueId (-1).
  int32_t value = content::kInvalidChildProcessUniqueId;
  if (process_id) {
    value = process_id.value();
  }
  return content::Java_ChildProcessId_Constructor(env, value);
}

}  // namespace jni_zero

DEFINE_JNI(ChildProcessId)
