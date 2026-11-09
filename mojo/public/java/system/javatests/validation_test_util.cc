// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_bytebuffer.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/test_support_android.h"
#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/mojo_javatests_jni/ValidationTestUtil_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace mojo {
namespace android {

static ScopedJavaLocalRef<jobject> JNI_ValidationTestUtil_ParseData(
    JNIEnv* env,
    const JavaRef<jstring>& data_as_string) {
  std::string input =
      base::android::ConvertJavaStringToUTF8(env, data_as_string);
  std::vector<uint8_t> data;
  size_t num_handles;
  std::string error_message;
  if (!test::ParseValidationTestInput(input, &data, &num_handles,
                                      &error_message)) {
    ScopedJavaLocalRef<jstring> j_error_message =
        base::android::ConvertUTF8ToJavaString(env, error_message);
    return Java_ValidationTestUtil_buildData(env, nullptr, 0, j_error_message);
  }
  ScopedJavaLocalRef<jobject> byte_buffer = jni_zero::ByteBufferAllocateDirect(
      env, base::checked_cast<int>(data.size()));
  CHECK(byte_buffer);
  base::android::JavaByteBufferToMutableSpan(env, byte_buffer).copy_from(data);
  return Java_ValidationTestUtil_buildData(env, byte_buffer, num_handles,
                                           nullptr);
}

}  // namespace android
}  // namespace mojo

DEFINE_JNI(ValidationTestUtil)
