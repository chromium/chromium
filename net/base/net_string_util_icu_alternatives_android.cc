// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "net/base/net_string_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/net_jni_headers/NetStringUtil_jni.h"

using base::android::ScopedJavaLocalRef;

namespace net {

namespace {

// Attempts to convert |text| encoded in |charset| to a jstring (Java unicode
// string).  Returns the result jstring, or NULL on failure.
ScopedJavaLocalRef<jstring> ConvertToJstring(std::string_view text,
                                             const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_byte_buffer(
      env,
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length()));
  base::android::CheckException(env);
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, std::string_view(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicode(env, java_byte_buffer,
                                                   java_charset);
  return java_result;
}

// Attempts to convert |text| encoded in |charset| to a jstring (Java unicode
// string) and then normalizes the string.  Returns the result jstring, or NULL
// on failure.
ScopedJavaLocalRef<jstring> ConvertToNormalizedJstring(std::string_view text,
                                                       const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_byte_buffer(
      env,
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length()));
  base::android::CheckException(env);
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, std::string_view(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicodeAndNormalize(
          env, java_byte_buffer, java_charset);
  return java_result;
}

// Converts |text| encoded in |charset| to a jstring (Java unicode string).
// Any characters that can not be converted are replaced with U+FFFD.
ScopedJavaLocalRef<jstring> ConvertToJstringWithSubstitutions(
    std::string_view text,
    const char* charset) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_byte_buffer(
      env,
      env->NewDirectByteBuffer(const_cast<char*>(text.data()), text.length()));
  base::android::CheckException(env);
  base::android::ScopedJavaLocalRef<jstring> java_charset =
      base::android::ConvertUTF8ToJavaString(env, std::string_view(charset));
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_convertToUnicodeWithSubstitutions(
          env, java_byte_buffer, java_charset);
  return java_result;
}

}  // namespace

// This constant cannot be defined as const char[] because it is initialized
// by base::kCodepageLatin1 (which is const char[]) in net_string_util_icu.cc.
const char* const kCharsetLatin1 = "ISO-8859-1";

bool ConvertToUtf8(std::string_view text,
                   const char* charset,
                   std::string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToJstring(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF8(java_result);
  return true;
}

bool ConvertToUtf8AndNormalize(std::string_view text,
                               const char* charset,
                               std::string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToNormalizedJstring(
      text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF8(java_result);
  return true;
}

bool ConvertToUTF16(std::string_view text,
                    const char* charset,
                    std::u16string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result = ConvertToJstring(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF16(java_result);
  return true;
}

bool ConvertToUTF16WithSubstitutions(std::string_view text,
                                     const char* charset,
                                     std::u16string* output) {
  output->clear();
  ScopedJavaLocalRef<jstring> java_result =
      ConvertToJstringWithSubstitutions(text, charset);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF16(java_result);
  return true;
}

bool ToUpperUsingLocale(std::u16string_view str, std::u16string* output) {
  output->clear();
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_new_str(
      env,
      env->NewString(reinterpret_cast<const jchar*>(str.data()), str.length()));
  if (java_new_str.is_null())
    return false;
  ScopedJavaLocalRef<jstring> java_result =
      android::Java_NetStringUtil_toUpperCase(env, java_new_str);
  if (java_result.is_null())
    return false;
  *output = base::android::ConvertJavaStringToUTF16(java_result);
  return true;
}

}  // namespace net
