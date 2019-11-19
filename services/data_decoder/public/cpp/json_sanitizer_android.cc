// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/json_sanitizer.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/data_decoder/public/cpp/android/safe_json_jni_headers/JsonSanitizer_jni.h"

using base::android::JavaParamRef;

// This file contains an implementation of JsonSanitizer that calls into Java.
// It deals with malformed input (in particular malformed Unicode encodings) in
// the following steps:
// 1. The input string is checked for whether it is well-formed UTF-8. Malformed
//    UTF-8 is rejected.
// 2. The UTF-8 string is converted in native code to a Java String, which is
//    encoded as UTF-16.
// 2. The Java String is parsed as JSON in the memory-safe environment of the
//    Java VM and any string literals are unescaped.
// 3. The string literals themselves are now untrusted, so they are checked in
//    Java for whether they are valid UTF-16.
// 4. The parsed JSON with sanitized literals is encoded back into a Java
//    String and passed back to native code.
// 5. The Java String is converted back to UTF-8 in native code.
// This ensures that both invalid UTF-8 and invalid escaped UTF-16 will be
// rejected.

namespace data_decoder {

// static
void JsonSanitizer::Sanitize(const std::string& json, Callback callback) {
  // The JSON parser only accepts wellformed UTF-8.
  if (!base::IsStringUTF8(json)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  Result::Error("Unsupported encoding")));
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> json_java =
      base::android::ConvertUTF8ToJavaString(env, json);

  // NOTE: This will *synchronously* invoke either the
  // |JNI_JsonSanitizer_OnSuccess| or |JNI_JsonSanitizer_OnError| function
  // below, so passing the address of |callback| is safe.
  Java_JsonSanitizer_sanitize(env, reinterpret_cast<jlong>(&callback),
                              json_java);
}

void JNI_JsonSanitizer_OnSuccess(JNIEnv* env,
                                 jlong jcallback,
                                 const JavaParamRef<jstring>& json) {
  auto* callback = reinterpret_cast<JsonSanitizer::Callback*>(jcallback);
  JsonSanitizer::Result result;
  result.value = base::android::ConvertJavaStringToUTF8(env, json);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback), std::move(result)));
}

void JNI_JsonSanitizer_OnError(JNIEnv* env,
                               jlong jcallback,
                               const JavaParamRef<jstring>& error) {
  auto* callback = reinterpret_cast<JsonSanitizer::Callback*>(jcallback);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(*callback),
                     JsonSanitizer::Result::Error(
                         base::android::ConvertJavaStringToUTF8(env, error))));
}

}  // namespace data_decoder
