// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/android/message_payload.h"

#include <optional>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"

namespace {

// An ArrayBufferPayload impl for Browser (Java) to JavaScript message, the
// ArrayBuffer payload data is stored in a Java byte array.
class JavaArrayBuffer : public blink::WebMessageArrayBufferPayload {
 public:
  explicit JavaArrayBuffer(const base::android::JavaRef<jbyteArray>& array)
      : length_(base::android::SafeGetArrayLength(
            base::android::AttachCurrentThread(),
            array)),
        array_(array) {}

  size_t GetLength() const override { return length_; }

  // Java ArrayBuffers are always fixed-length.
  bool GetIsResizableByUserJavaScript() const override { return false; }

  size_t GetMaxByteLength() const override { return length_; }

  // Due to JNI limitation, Java ByteArray cannot be converted into base::span
  // trivially.
  std::optional<base::span<const uint8_t>> GetAsSpanIfPossible()
      const override {
    return std::nullopt;
  }

  void CopyInto(base::span<uint8_t> dest) const override {
    base::android::JavaByteArrayToByteSpan(base::android::AttachCurrentThread(),
                                           array_, dest);
  }

 private:
  size_t length_;
  base::android::ScopedJavaGlobalRef<jbyteArray> array_;
};
}  // namespace

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
          [env](const std::unique_ptr<blink::WebMessageArrayBufferPayload>&
                    array_buffer) {
            // Data is from renderer process, copy it first before use.
            base::android::ScopedJavaLocalRef<jbyteArray> j_byte_array;

            auto span_optional = array_buffer->GetAsSpanIfPossible();
            if (span_optional) {
              j_byte_array =
                  base::android::ToJavaByteArray(env, span_optional.value());
            } else {
              // The ArrayBufferPayload impl does not support |GetArrayBuffer|.
              // Fallback to allocate a temporary buffer and copy the data.
              std::vector<uint8_t> data(array_buffer->GetLength());
              array_buffer->CopyInto(data);
              j_byte_array = base::android::ToJavaByteArray(env, data);
            }

            return Java_MessagePayloadJni_createFromArrayBuffer(env,
                                                                j_byte_array);
          }},
      payload);
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
      return std::make_unique<JavaArrayBuffer>(byte_array);
    }
    case MessagePayloadType::kInvalid:
      break;
  }
  NOTREACHED_IN_MIGRATION()
      << "Unsupported or invalid Java MessagePayload type.";
  return std::u16string();
}

}  // namespace content::android
