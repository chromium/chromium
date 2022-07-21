// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/android/message_payload.h"

#include <string>
#include <utility>
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/android/content_jni_headers/MessagePayloadJni_jni.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

base::android::ScopedJavaLocalRef<jobject> CreateJavaMessagePayload(
    const blink::TransferableMessage& transferable_message) {
  absl::optional<blink::WebMessagePayload> optional_payload =
      blink::DecodeToWebMessagePayload(transferable_message);
  if (!optional_payload) {
    // Unsupported message or decode failed.
    return nullptr;
  }
  auto& payload = optional_payload.value();
  if (!absl::holds_alternative<std::u16string>(payload)) {
    // TODO: Add support for ArrayBuffer.
    return nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MessagePayloadJni_createFromString(
      env, base::android::ConvertUTF16ToJavaString(
               env, absl::get<std::u16string>(payload)));
}

blink::TransferableMessage CreateTransferableMessageFromJavaMessagePayload(
    const base::android::ScopedJavaLocalRef<jobject>& java_message) {
  auto string = base::android::ConvertJavaStringToUTF16(
      Java_MessagePayloadJni_getAsString(base::android::AttachCurrentThread(),
                                         java_message));
  return blink::EncodeWebMessagePayload(
      blink::WebMessagePayload(std::move(string)));
}

}  // namespace content::android
