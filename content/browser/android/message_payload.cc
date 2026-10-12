// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/public/browser/android/message_payload.h"

#include <android/sharedmem.h>
#include <android/sharedmem_jni.h>

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "content/public/android/content_jni_headers/SharedArrayBuffer_jni.h"

namespace {

// An ArrayBufferPayload impl for Browser (Java) to JavaScript message, the
// ArrayBuffer payload data is stored in a Java byte array.
class JavaArrayBuffer : public blink::WebMessageArrayBufferPayload {
 public:
  explicit JavaArrayBuffer(const base::android::JavaRef<jbyteArray>& array)
      : length_(static_cast<size_t>(
            array.GetLength(base::android::AttachCurrentThread()))),
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
  return std::visit(
      absl::Overload{
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
          },
          [](const base::UnsafeSharedMemoryRegion& shared_memory)
              -> base::android::ScopedJavaLocalRef<jobject> {
            NOTREACHED() << "SharedArrayBuffer cannot be converted to Java "
                            "MessagePayload.";
          }},
      payload);
}

static base::UnsafeSharedMemoryRegion ConvertJavaSharedMemoryToUnsafeRegion(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_payload_shared_array_buffer) {
  if (!j_payload_shared_array_buffer) {
    return base::UnsafeSharedMemoryRegion();
  }

  base::android::ScopedJavaLocalRef<jobject> j_shared_memory =
      Java_SharedArrayBuffer_getSharedMemory(env,
                                             j_payload_shared_array_buffer);
  if (!j_shared_memory) {
    return base::UnsafeSharedMemoryRegion();
  }

  base::ScopedFD scoped_fd(
      ASharedMemory_dupFromJava(env, j_shared_memory.obj()));
  if (!scoped_fd.is_valid()) {
    return base::UnsafeSharedMemoryRegion();
  }

  size_t size = ASharedMemory_getSize(scoped_fd.get());
  if (size == 0) {
    return base::UnsafeSharedMemoryRegion();
  }

  // We use Mode::kUnsafe because any process with the FD can write to it.
  base::subtle::PlatformSharedMemoryRegion platform_region =
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(scoped_fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, size,
          base::UnguessableToken::Create());

  if (!platform_region.IsValid()) {
    return base::UnsafeSharedMemoryRegion();
  }

  return base::UnsafeSharedMemoryRegion::Deserialize(
      std::move(platform_region));
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
    case MessagePayloadType::kSharedArrayBuffer: {
      auto j_shared_array_buffer =
          Java_MessagePayloadJni_getAsSharedArrayBuffer(env, java_message);

      base::UnsafeSharedMemoryRegion region =
          ConvertJavaSharedMemoryToUnsafeRegion(env, j_shared_array_buffer);
      return std::move(region);
    }
    case MessagePayloadType::kInvalid:
      break;
  }
  NOTREACHED() << "Unsupported or invalid Java MessagePayload type.";
}

}  // namespace content::android

DEFINE_JNI(MessagePayloadJni)
DEFINE_JNI(SharedArrayBuffer)
