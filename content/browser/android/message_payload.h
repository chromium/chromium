// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_
#define CONTENT_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"

namespace content::android {

// Helper methods to convert between java
// `org.chromium.content_public.browser.MessagePayload` and
// `blink::TransferableMessage`. Only payload data (String, ArrayBuffer etc) is
// converted, the rest in `TransferableMessage` (like MessagePort) is not
// handled.

// Construct Java `org.chromium.content_public.browser.MessagePayload` from
// `blink::TransferableMessage`.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobject>
CreateJavaMessagePayload(const blink::TransferableMessage&);

// Construct `blink::TransferableMessage` from Java MessagePayload.
CONTENT_EXPORT blink::TransferableMessage
CreateTransferableMessageFromJavaMessagePayload(
    const base::android::ScopedJavaLocalRef<
        jobject>& /* org.chromium.content_public.browser.MessagePayload */);

}  // namespace content::android

#endif  // CONTENT_BROWSER_ANDROID_MESSAGE_PAYLOAD_H_
