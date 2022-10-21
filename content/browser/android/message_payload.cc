// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/android/message_payload.h"

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "components/js_injection/common/interfaces.mojom-forward.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

base::android::ScopedJavaLocalRef<jobject> ConvertWebMessagePayloadToJava(
    const blink::WebMessagePayload& payload) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return absl::visit(
      base::Overloaded{
          [env](const std::u16string& str) {
            return Java_MessagePayloadJni_createFromString(
                env, base::android::ConvertUTF16ToJavaString(env, str));
          },
          [env](const std::vector<uint8_t>& array_buffer) {
            return Java_MessagePayloadJni_createFromArrayBuffer(
                env, base::android::ToJavaByteArray(env, array_buffer.data(),
                                                    array_buffer.size()));
          },
      },
      payload);
}

base::android::ScopedJavaLocalRef<jobject> ConvertJsWebMessagePtrToJava(
    js_injection::mojom::JsWebMessagePtr message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (message->is_string_value()) {
    return Java_MessagePayloadJni_createFromString(
        env, base::android::ConvertUTF16ToJavaString(
                 env, message->get_string_value()));
  } else if (message->is_array_buffer_value()) {
    auto& big_buffer = message->get_array_buffer_value();
    return Java_MessagePayloadJni_createFromArrayBuffer(
        env, base::android::ToJavaByteArray(env, big_buffer.data(),
                                            big_buffer.size()));
  } else {
    return nullptr;
  }
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

CONTENT_EXPORT js_injection::mojom::JsWebMessagePtr
ConvertToJsWebMessagePtrFromJava(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  CHECK(java_message);
  JNIEnv* env = base::android::AttachCurrentThread();
  const MessagePayloadType type = static_cast<MessagePayloadType>(
      Java_MessagePayloadJni_getType(env, java_message));
  switch (type) {
    case MessagePayloadType::kString: {
      return js_injection::mojom::JsWebMessage::NewStringValue(
          base::android::ConvertJavaStringToUTF16(
              Java_MessagePayloadJni_getAsString(env, java_message)));
    }
    case MessagePayloadType::kArrayBuffer: {
      auto byte_array =
          Java_MessagePayloadJni_getAsArrayBuffer(env, java_message);
      mojo_base::BigBuffer buffer(env->GetArrayLength(byte_array.obj()));
      env->GetByteArrayRegion(byte_array.obj(), 0, buffer.size(),
                              reinterpret_cast<jbyte*>(buffer.data()));
      return js_injection::mojom::JsWebMessage::NewArrayBufferValue(
          std::move(buffer));
    }
    case MessagePayloadType::kInvalid:
      NOTREACHED() << "Unsupported or invalid Java MessagePayload type.";
      return js_injection::mojom::JsWebMessagePtr();
  }
}

}  // namespace content::android
