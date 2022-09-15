// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_TYPE_H_

namespace content::android {

// Payload types for Java `org.chromium.content_public.browser.MessagePayload`
// and JNI.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content_public.browser
enum class MessagePayloadType {
  kInvalid = -1,
  kString,
  kArrayBuffer,
};

}  // namespace content::android
#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_TYPE_H_
