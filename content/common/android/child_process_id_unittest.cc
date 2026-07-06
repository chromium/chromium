// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/child_process_id.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ChildProcessIdTest, JniConversions) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Test standard valid ID conversion roundtrip.
  content::ChildProcessId original =
      content::ChildProcessId::FromUnsafeValue(42);
  base::android::ScopedJavaLocalRef<jobject> j_obj =
      jni_zero::ToJniType(env, original);
  ASSERT_TRUE(j_obj);

  content::ChildProcessId converted =
      jni_zero::FromJniType<content::ChildProcessId>(env, j_obj);
  EXPECT_EQ(original, converted);
}

TEST(ChildProcessIdTest, NullConversion) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Passing a null Java reference should return an invalid ChildProcessId (-1).
  base::android::JavaRef<jobject> null_ref;
  content::ChildProcessId converted =
      jni_zero::FromJniType<content::ChildProcessId>(env, null_ref);
  EXPECT_TRUE(converted.is_null());
}

TEST(ChildProcessIdTest, InvalidIdRoundtrip) {
  JNIEnv* env = base::android::AttachCurrentThread();

  content::ChildProcessId invalid_id;  // Defaults to -1 (invalid)
  base::android::ScopedJavaLocalRef<jobject> j_obj =
      jni_zero::ToJniType(env, invalid_id);
  ASSERT_TRUE(j_obj);

  content::ChildProcessId converted =
      jni_zero::FromJniType<content::ChildProcessId>(env, j_obj);
  EXPECT_EQ(invalid_id, converted);
  EXPECT_TRUE(converted.is_null());
}

}  // namespace content
