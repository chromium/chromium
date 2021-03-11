// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/app_web_message_port.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "content/public/android/content_jni_headers/AppWebMessagePort_jni.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

namespace content {

namespace AppWebMessagePort {

// static
std::vector<blink::MessagePortDescriptor> UnwrapJavaArray(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jports) {
  std::vector<blink::MessagePortDescriptor> ports;
  if (!jports.is_null()) {
    for (auto jport : jports.ReadElements<jobject>()) {
      jlong port_ptr =
          Java_AppWebMessagePort_releaseNativeMessagePortDescriptor(env, jport);
      // Ports are heap allocated native objects. Since we are taking ownership
      // of the object from the Java code we are responsible for cleaning it up.
      std::unique_ptr<blink::MessagePortDescriptor> port = base::WrapUnique(
          reinterpret_cast<blink::MessagePortDescriptor*>(port_ptr));
      ports.push_back(std::move(*port));
    }
  }
  return ports;
}

// static
base::android::ScopedJavaGlobalRef<jobjectArray> WrapJavaArray(
    JNIEnv* env,
    std::vector<blink::MessagePortDescriptor> descriptors) {
  // Convert to an array of raw blink::MessagePortDescriptor pointers. Ownership
  // of these objects is passed to Java.
  std::vector<int64_t> descriptor_ptrs;
  descriptor_ptrs.reserve(descriptors.size());
  for (size_t i = 0; i < descriptors.size(); ++i) {
    blink::MessagePortDescriptor* descriptor =
        new blink::MessagePortDescriptor(std::move(descriptors[i]));
    descriptor_ptrs.push_back(reinterpret_cast<int64_t>(descriptor));
  }

  // Now convert to a Java array.
  base::android::ScopedJavaLocalRef<jlongArray> native_descriptors =
      base::android::ToJavaLongArray(env, descriptor_ptrs.data(),
                                     descriptor_ptrs.size());

  // And finally convert this to a Java array of AppWebMessagePorts.
  base::android::ScopedJavaLocalRef<jobjectArray> ports =
      Java_AppWebMessagePort_createFromNativeBlinkMessagePortDescriptors(
          env, native_descriptors);

  base::android::ScopedJavaGlobalRef<jobjectArray> global_ports(ports);
  return global_ports;
}

}  // namespace AppWebMessagePort

base::android::ScopedJavaLocalRef<jstring>
JNI_AppWebMessagePort_DecodeStringMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& encoded_data) {
  std::vector<uint8_t> encoded_message;
  base::android::JavaByteArrayToByteVector(env, encoded_data, &encoded_message);

  std::u16string message;
  if (!blink::DecodeStringMessage(encoded_message, &message))
    return nullptr;

  base::android::ScopedJavaLocalRef<jstring> jmessage =
      base::android::ConvertUTF16ToJavaString(env, message);
  return jmessage;
}

base::android::ScopedJavaLocalRef<jbyteArray>
JNI_AppWebMessagePort_EncodeStringMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jmessage) {
  std::vector<uint8_t> encoded_message = blink::EncodeStringMessage(
      base::android::ConvertJavaStringToUTF16(jmessage));
  return base::android::ToJavaByteArray(env, encoded_message);
}

}  // namespace content
