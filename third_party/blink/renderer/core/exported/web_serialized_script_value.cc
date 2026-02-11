/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/web/web_serialized_script_value.h"

#include "base/check.h"
#include "mojo/public/cpp/system/message.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message_mojom_traits.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WebSerializedScriptValue WebSerializedScriptValue::Serialize(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value) {
  DummyExceptionStateForTesting exception_state;
  WebSerializedScriptValue serialized_value = SerializedScriptValue::Serialize(
      isolate, value, SerializedScriptValue::SerializeOptions(),
      exception_state);
  if (exception_state.HadException())
    return CreateInvalid();
  return serialized_value;
}

// static
WebSerializedScriptValue WebSerializedScriptValue::CreateFromCloneableMessage(
    CloneableMessage message) {
  mojo::Message mojo_message =
      mojom::blink::CloneableMessage::SerializeAsMessage(&message);

  // Reconstruct the message to ensure handles are properly attached.
  mojo::ScopedMessageHandle handle = mojo_message.TakeMojoMessage();
  mojo_message = mojo::Message::CreateFromMessageHandle(&handle);

  BlinkCloneableMessage blink_message;
  if (!mojom::blink::CloneableMessage::DeserializeFromMessage(
          std::move(mojo_message), &blink_message)) {
    return CreateInvalid();
  }
  return WebSerializedScriptValue(std::move(blink_message.message));
}

CloneableMessage WebSerializedScriptValue::GetCloneableMessage(
    base::UnguessableToken sender_agent_cluster_id) const {
  BlinkCloneableMessage blink_message;
  blink_message.message = private_.Get();
  // The mojo serializer requires `sender_agent_cluster_id` to be non-empty.
  blink_message.sender_agent_cluster_id = std::move(sender_agent_cluster_id);

  mojo::Message mojo_message =
      mojom::blink::CloneableMessage::SerializeAsMessage(&blink_message);

  // Reconstruct the message to ensure handles are properly attached.
  mojo::ScopedMessageHandle handle = mojo_message.TakeMojoMessage();
  mojo_message = mojo::Message::CreateFromMessageHandle(&handle);

  CloneableMessage message;
  CHECK(mojom::blink::CloneableMessage::DeserializeFromMessage(
      std::move(mojo_message), &message));
  return message;
}

WebSerializedScriptValue WebSerializedScriptValue::CreateInvalid() {
  return SerializedScriptValue::Create();
}

void WebSerializedScriptValue::Reset() {
  private_.Reset();
}

void WebSerializedScriptValue::Assign(const WebSerializedScriptValue& other) {
  private_ = other.private_;
}

bool WebSerializedScriptValue::IsValid() const {
  // Must have an underlying SerializedScriptValue object.
  if (private_.IsNull()) {
    return false;
  }

  // That object must have wire data. `CreateInvalid()` produces an empty
  // buffer, whereas any valid serialization (even of JS `null`, `undefined`, or
  // an empty string) always has a header.
  return !private_->GetWireData().empty();
}

v8::Local<v8::Value> WebSerializedScriptValue::Deserialize(
    v8::Isolate* isolate) {
  return private_->Deserialize(isolate);
}

WebSerializedScriptValue::WebSerializedScriptValue(
    scoped_refptr<SerializedScriptValue> value)
    : private_(std::move(value)) {}

WebSerializedScriptValue& WebSerializedScriptValue::operator=(
    scoped_refptr<SerializedScriptValue> value) {
  private_ = std::move(value);
  return *this;
}

WebSerializedScriptValue::operator scoped_refptr<SerializedScriptValue>()
    const {
  return private_.Get();
}

}  // namespace blink
