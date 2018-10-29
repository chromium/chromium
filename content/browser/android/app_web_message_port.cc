// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/app_web_message_port.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/AppWebMessagePort_jni.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

using blink::MessagePortChannel;

namespace content {

// static
std::vector<blink::MessagePortChannel> AppWebMessagePort::UnwrapJavaArray(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jports) {
  std::vector<blink::MessagePortChannel> channels;
  if (!jports.is_null()) {
    jsize num_ports = env->GetArrayLength(jports.obj());
    for (jsize i = 0; i < num_ports; ++i) {
      base::android::ScopedJavaLocalRef<jobject> jport(
          env, env->GetObjectArrayElement(jports.obj(), i));
      jint native_port = Java_AppWebMessagePort_releaseNativeHandle(env, jport);
      channels.push_back(blink::MessagePortChannel(
          mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(native_port))));
    }
  }
  return channels;
}

base::android::ScopedJavaLocalRef<jstring>
JNI_AppWebMessagePort_DecodeStringMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jbyteArray>& encoded_data) {
  std::vector<uint8_t> encoded_message;
  base::android::JavaByteArrayToByteVector(env, encoded_data, &encoded_message);

  base::string16 message;
  if (!blink::DecodeStringMessage(encoded_message, &message))
    return nullptr;

  base::android::ScopedJavaLocalRef<jstring> jmessage =
      base::android::ConvertUTF16ToJavaString(env, message);
  return jmessage;
}

base::android::ScopedJavaLocalRef<jbyteArray>
JNI_AppWebMessagePort_EncodeStringMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jstring>& jmessage) {
  std::vector<uint8_t> encoded_message = blink::EncodeStringMessage(
      base::android::ConvertJavaStringToUTF16(jmessage));
  return base::android::ToJavaByteArray(env, encoded_message);
}

}  // namespace content
