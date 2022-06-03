// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace content {
namespace AppWebMessagePort {

// Helper functions for converting between arrays of
// blink::MessagePortDescriptors (the way message ports are passed around in
// C++) and Java arrays of AppWebMessagePorts (the way they are passed around
// in Java). This is exposed for embedders (Android Webview, for example).

// Given an array of AppWebMessagePort objects, unwraps them and returns an
// equivalent array of blink::MessagePortDescriptors.
CONTENT_EXPORT std::vector<blink::MessagePortDescriptor> UnwrapJavaArray(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jports);

// Given an array of blink::MessagePortDescriptor objects, wraps them and
// returns an equivalent array of AppWebMessagePort objects.
CONTENT_EXPORT base::android::ScopedJavaGlobalRef<jobjectArray> WrapJavaArray(
    JNIEnv* env,
    std::vector<blink::MessagePortDescriptor> descriptors);

}  // namespace AppWebMessagePort
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_