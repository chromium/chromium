// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PORT_HELPER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PORT_HELPER_H_

#include <vector>
#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace content::android {

// Helper function for converting between arrays of
// `blink::MessagePortDescriptors` (the way message ports are passed around in
// C++) and Java arrays of MessagePorts (the way they are passed around
// in Java).

// Take the ownership of `MessagePortDescriptor`s, and create a java array of
// `org.chromium.content_public.browser.MessagePort` to wrap them.
CONTENT_EXPORT base::android::ScopedJavaLocalRef<jobjectArray>
    CreateJavaMessagePort(std::vector<blink::MessagePortDescriptor>);

}  // namespace content::android

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_MESSAGE_PORT_HELPER_H_
