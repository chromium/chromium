// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/app_web_message_port.h"
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/android/message_payload.h"
#include "content/public/browser/android/message_port_helper.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/AppWebMessagePort_jni.h"

namespace content::android {

base::android::ScopedJavaLocalRef<jobjectArray> CreateJavaMessagePort(
    std::vector<blink::MessagePortDescriptor> descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<base::android::ScopedJavaLocalRef<jobject>> j_descriptors;
  j_descriptors.reserve(descriptors.size());
  for (auto& descriptor : descriptors) {
    j_descriptors.push_back(AppWebMessagePort::Create(std::move(descriptor)));
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ToTypedJavaArrayOfObjects(
      env, base::make_span(j_descriptors),
      org_chromium_content_browser_AppWebMessagePort_clazz(env));
}

// static
base::android::ScopedJavaLocalRef<jobject> AppWebMessagePort::Create(
    blink::MessagePortDescriptor&& descriptor) {
  auto app_web_message_port =
      base::WrapUnique(new AppWebMessagePort(std::move(descriptor)));
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* app_web_messge_port_ptr = app_web_message_port.get();
  auto j_obj = Java_AppWebMessagePort_Constructor(
      env, reinterpret_cast<intptr_t>(app_web_message_port.release()));
  app_web_messge_port_ptr->j_obj_ = JavaObjectWeakGlobalRef(env, j_obj);
  return j_obj;
}

// static
std::vector<blink::MessagePortDescriptor> AppWebMessagePort::Release(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& jports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<blink::MessagePortDescriptor> ports;
  if (!jports.is_null()) {
    for (auto jport : jports.ReadElements<jobject>()) {
      jlong port_ptr = Java_AppWebMessagePort_getNativeObj(env, jport);
      // Ports are heap allocated native objects. Since we are taking ownership
      // of the object from the Java code we are responsible for cleaning it up.
      std::unique_ptr<AppWebMessagePort> port =
          base::WrapUnique(reinterpret_cast<AppWebMessagePort*>(port_ptr));
      ports.emplace_back(port->PassPort());
    }
  }
  return ports;
}

AppWebMessagePort::AppWebMessagePort(blink::MessagePortDescriptor&& descriptor)
    : runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      descriptor_(std::move(descriptor)) {
  // AppWebMessagePort can only be created on main thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  connector_ = std::make_unique<mojo::Connector>(
      descriptor_.TakeHandleToEntangleWithEmbedder(),
      mojo::Connector::SINGLE_THREADED_SEND);
  connector_->set_connection_error_handler(
      base::BindOnce(&AppWebMessagePort::OnPipeError, base::Unretained(this)));
}

AppWebMessagePort::~AppWebMessagePort() {
  DCHECK(runner_->BelongsToCurrentThread());
  GiveDisentangledHandleIfNeeded();
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppWebMessagePort_nativeDestroyed(env, GetJavaObj(env));
}

// JNI
void AppWebMessagePort::PostMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_message_payload,
    const base::android::JavaParamRef<jobjectArray>& j_ports) {
  DCHECK(runner_->BelongsToCurrentThread());
  DCHECK(descriptor_.IsValid());
  DCHECK(connector_);
  if (connector_->encountered_error()) {
    LOG(ERROR)
        << "Failed to send message to renderer, connector encountered error.";
    return;
  }
  blink::TransferableMessage transferable_message =
      blink::EncodeWebMessagePayload(ConvertToWebMessagePayloadFromJava(
          base::android::ScopedJavaLocalRef<jobject>(j_message_payload)));
  transferable_message.ports =
      blink::MessagePortChannel::CreateFromHandles(Release(env, j_ports));
  // As the message is posted from an Android app and not from another renderer,
  // set the agent cluster ID to the embedder's, and nullify its parent task ID.
  transferable_message.sender_agent_cluster_id =
      blink::WebMessagePort::GetEmbedderAgentClusterID();
  transferable_message.parent_task_id = std::nullopt;

  mojo::Message mojo_message =
      blink::mojom::TransferableMessage::SerializeAsMessage(
          &transferable_message);
  bool send_result = connector_->Accept(&mojo_message);
  DCHECK(send_result);
}

void AppWebMessagePort::SetShouldReceiveMessages(JNIEnv* env,
                                                 bool should_receive_message) {
  DCHECK(runner_->BelongsToCurrentThread());
  DCHECK(connector_);
  if (!should_receive_message) {
    connector_->set_incoming_receiver(nullptr);
    j_strong_obj_.Reset();
  } else {
    connector_->set_incoming_receiver(this);
    if (!connector_errored_) {
      j_strong_obj_ = j_obj_.get(env);
    }
    if (!is_watching_) {
      is_watching_ = true;
      connector_->StartReceiving(runner_);
    }
  }
}

void AppWebMessagePort::CloseAndDestroy(JNIEnv* env) {
  DCHECK(runner_->BelongsToCurrentThread());
  DCHECK(connector_);
  delete this;
}

// mojo::MessageReceiver:
bool AppWebMessagePort::Accept(mojo::Message* message) {
  DCHECK(runner_->BelongsToCurrentThread());
  blink::TransferableMessage transferable_message;
  if (!blink::mojom::TransferableMessage::DeserializeFromMessage(
          std::move(*message), &transferable_message)) {
    // Decode mojo message failed.
    return false;
  }
  auto ports = std::move(transferable_message.ports);
  auto optional_payload =
      blink::DecodeToWebMessagePayload(std::move(transferable_message));
  if (!optional_payload) {
    // Unsupported or invalid payload.
    return true;
  }
  const auto& payload = optional_payload.value();

  auto j_ports =
      CreateJavaMessagePort(blink::MessagePortChannel::ReleaseHandles(ports));
  base::android::ScopedJavaLocalRef<jobject> j_message =
      ConvertWebMessagePayloadToJava(payload);
  DCHECK(j_message);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppWebMessagePort_onMessage(env, GetJavaObj(env), j_message, j_ports);
  return true;
}

blink::MessagePortDescriptor AppWebMessagePort::PassPort() {
  DCHECK(runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AppWebMessagePort_setTransferred(env, GetJavaObj(env));
  GiveDisentangledHandleIfNeeded();
  return std::move(descriptor_);
}

void AppWebMessagePort::OnPipeError() {
  DCHECK(runner_->BelongsToCurrentThread());
  connector_errored_ = true;
  j_strong_obj_.Reset();
}

void AppWebMessagePort::GiveDisentangledHandleIfNeeded() {
  DCHECK(runner_->BelongsToCurrentThread());
  if (!connector_ || !descriptor_.IsValid()) {
    return;
  }
  if (connector_errored_) {
    // Return an empty message pipe.
    descriptor_.GiveDisentangledHandle(mojo::ScopedMessagePipeHandle());
  } else {
    descriptor_.GiveDisentangledHandle(connector_->PassMessagePipe());
  }
  connector_.reset();
}

base::android::ScopedJavaLocalRef<jobjectArray>
JNI_AppWebMessagePort_CreatePair(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::MessagePortDescriptorPair port_pair;
  std::vector<blink::MessagePortDescriptor> descriptors;
  descriptors.emplace_back(port_pair.TakePort0());
  descriptors.emplace_back(port_pair.TakePort1());
  return CreateJavaMessagePort(std::move(descriptors));
}

}  // namespace content::android
