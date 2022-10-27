// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/message_payload_type.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

// Helper methods to convert between java
// `org.chromium.content_public.browser.MessagePayload` and
// `blink::WebMessagePayload`.

// Construct Java `org.chromium.content_public.browser.MessagePayload` from
// `blink::WebMessagePayload`.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
ConvertWebMessagePayloadToJava(const blink::WebMessagePayload& payload);

CONTENT_EXPORT blink::WebMessagePayload ConvertToWebMessagePayloadFromJava(
    const base::android::ScopedJavaLocalRef<
        jobject>& /* org.chromium.content_public.browser.MessagePayload */);

}  // namespace content::android

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_
