// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_CHILD_PROCESS_ID_H_
#define CONTENT_COMMON_ANDROID_CHILD_PROCESS_ID_H_

#include "base/android/scoped_java_ref.h"
#include "content/public/common/child_process_id.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
content::ChildProcessId FromJniType<content::ChildProcessId>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object);

template <>
ScopedJavaLocalRef<jobject> ToJniType<content::ChildProcessId>(
    JNIEnv* env,
    const content::ChildProcessId& process_id);

}  // namespace jni_zero

#endif  // CONTENT_COMMON_ANDROID_CHILD_PROCESS_ID_H_
