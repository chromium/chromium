// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_
#define CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

namespace mojo {
class Connector;
class Message;
}  // namespace mojo

namespace content::android {

// This is the native side of the java `AppWebMessagePort`, which is present
// while the port is still bound to the java object. While bound, the lifetime
// of the native side is owned by the java side. Creation: The only way to
// create this object is via `content::MessagePort::CreateJavaMessagePort`. The
// native object will be created first, and Java object is created within the
// constructor of native object.
//
// Destruction:
// 1. Pass the ownership to `blink::MessagePortDescriptor` with
// `Release`. This unbinds the message port from the java
// object and moves it to `MessagePortDescriptor` to be passed in a message.
// Native `AppWebMessagePort` object is deleted as part of this operation.
// 2. Close the MessagePort in Java.
// 3. The Java object is garbage collected, but `close` is not called.
//
// Threading: Methods should be called on UI thread only.
class CONTENT_EXPORT AppWebMessagePort : public mojo::MessageReceiver {
 public:
  // Create `AppWebMessagePort` from a `MessagePortDescriptor`, this takes the
  // ownership of `MessagePortDescriptor`. `AppWebMessagePort` is created and
  // returned.
  static base::android::ScopedJavaLocalRef<jobject> Create(
      blink::MessagePortDescriptor&& descriptor);

  // Given an array of `AppWebMessagePort` objects, unwraps them and returns an
  // equivalent array of `blink::MessagePortDescriptors`. The underlying
  // instances(Java and Native) will be destroyed.
  static std::vector<blink::MessagePortDescriptor> Release(
      JNIEnv* env,
      const base::android::JavaRef<jobjectArray>&
          jports /* org.chromium.content.browser.AppWebMessagePort */);

  // When clean up native, we notify Java instance to release native handle.
  ~AppWebMessagePort() override;

  void PostMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_message_payload,
      const base::android::JavaParamRef<jobjectArray>& j_ports);
  void SetShouldReceiveMessages(JNIEnv* env, bool should_receive_message);
  void CloseAndDestroy(JNIEnv* env);

 private:
  explicit AppWebMessagePort(blink::MessagePortDescriptor&& descriptor);

  // Convert `j_obj_` weak reference to a strong local reference.
  // The Java object is always available until native object destroyed.
  // This assumption is still true even when the Java object is GC-ed, since
  // it's strong referenced again by PostTask.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObj(JNIEnv* env) {
    auto java_ref = j_obj_.get(env);
    DCHECK(java_ref);
    return java_ref;
  }

  // Pass out the port descriptor. This port will become invalid after calling
  // this. This can only be called if `PostMessage` and `SetMessageCallback` are
  // never called.
  blink::MessagePortDescriptor PassPort();

  // The error handler for the connector. This lets the instrumentation be aware
  // of the message pipe closing itself due to error, which happens
  // unconditionally. Note that a subsequent call to
  // `Connector::PassMessagePipe` should always return an invalid handle at that
  // point.
  // TODO(chrisha): Make this an immediate notification that the channel has
  // been torn down rather than waiting for the owning `MessagePort` to be
  // cleaned up.
  void OnPipeError();
  void GiveDisentangledHandleIfNeeded();

  // mojo::MessageReceiver:
  bool Accept(mojo::Message* message) override;

  // UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> runner_;

  blink::MessagePortDescriptor descriptor_;
  JavaObjectWeakGlobalRef j_obj_;

  // Set when this port is receiving messages. Port should be kept alive
  // as long as it can still receive messages.
  base::android::ScopedJavaGlobalRef<jobject> j_strong_obj_;

  bool connector_errored_ = false;
  bool is_watching_ = false;

  std::unique_ptr<mojo::Connector> connector_;
};

}  // namespace content::android

#endif  // CONTENT_BROWSER_ANDROID_APP_WEB_MESSAGE_PORT_H_
