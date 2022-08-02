// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/android/message_payload.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

namespace {
using base::android::ScopedJavaLocalRef;

// Helper methods to convert `blink::WebMessagePayload` to Java
// `org.chromium.content_public.browser.MessagePayload`.
struct PayloadToJavaVisitor {
  ScopedJavaLocalRef<jobject> operator()(const std::u16string& str) {
    return Java_MessagePayloadJni_createFromString(
        env_, base::android::ConvertUTF16ToJavaString(env_, str));
  }
  ScopedJavaLocalRef<jobject> operator()(
      const std::vector<uint8_t>& array_buffer) {
    return Java_MessagePayloadJni_createFromArrayBuffer(
        env_, base::android::ToJavaByteArray(env_, array_buffer.data(),
                                             array_buffer.size()));
  }

  explicit PayloadToJavaVisitor(JNIEnv* env) : env_(env){};
  JNIEnv* env_;
};

};  // namespace

base::android::ScopedJavaLocalRef<jobject> ConvertWebMessagePayloadToJava(
    const blink::WebMessagePayload& payload) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return absl::visit(PayloadToJavaVisitor(env), payload);
}

blink::WebMessagePayload ConvertToWebMessagePayloadFromJava(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  CHECK(java_message);
  JNIEnv* env = base::android::AttachCurrentThread();
  const MessagePayloadType type = static_cast<MessagePayloadType>(
      Java_MessagePayloadJni_getType(env, java_message));
  switch (type) {
    case MessagePayloadType::kString: {
      return base::android::ConvertJavaStringToUTF16(
          Java_MessagePayloadJni_getAsString(env, java_message));
    }
    case MessagePayloadType::kArrayBuffer: {
      auto byte_array =
          Java_MessagePayloadJni_getAsArrayBuffer(env, java_message);
      std::vector<uint8_t> vector;
      base::android::JavaByteArrayToByteVector(env, byte_array, &vector);
      return vector;
    }
    case MessagePayloadType::kInvalid:
      break;
  }
  NOTREACHED() << "Unsupported or invalid Java MessagePayload type.";
  return std::u16string();
}

}  // namespace content::android
