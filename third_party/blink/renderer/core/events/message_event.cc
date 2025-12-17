/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/events/message_event.h"

#include <memory>

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/url/dom_origin.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

static inline bool IsValidSource(EventTarget* source) {
  return !source || source->ToDOMWindow() || source->ToMessagePort() ||
         source->ToServiceWorker();
}

size_t MessageEvent::SizeOfExternalMemoryInBytes() {
  switch (data_type_) {
    case kDataTypeNull:
      return 0;
    case kDataTypeScriptValue:
      // This is not external memory.
      return 0;
    case kDataTypeSerializedScriptValue: {
      size_t result = 0;
      for (const auto& array_buffer :
           data_as_serialized_script_value_->ArrayBuffers()) {
        result += array_buffer->ByteLength();
      }

      return result;
    }
    case kDataTypeString:
      return data_as_string_.length();
    case kDataTypeBlob:
      return static_cast<size_t>(data_as_blob_->size());
    case kDataTypeArrayBuffer:
      return data_as_array_buffer_->ByteLength();
  }
}

DOMOrigin* MessageEvent::GetDOMOrigin(LocalDOMWindow*) const {
  // We only create `DOMOrigin` objects for `MessageEvent` objects that were not
  // constructed from JavaScript, as the JavaScript constructor accepts an
  // untrusted string serialization of an origin.
  if (!potentially_invalid_origin_serialization_.IsNull() ||
      !GetSecurityOrigin()) {
    return nullptr;
  }

  // No access check is required, as this object intentionally reveals its
  // sender's origin cross-origin.
  return DOMOrigin::Create(GetSecurityOrigin());
}

MessageEvent::MessageEvent() : data_type_(kDataTypeScriptValue) {}

MessageEvent::MessageEvent(const AtomicString& type,
                           const MessageEventInit* initializer)
    : Event(type, initializer),
      data_type_(kDataTypeScriptValue),
      source_(nullptr) {
  // TODO(crbug.com/1070964): Remove this existence check.  There is a bug that
  // the current code generator does not initialize a ScriptValue with the
  // v8::Null value despite that the dictionary member has the default value of
  // IDL null.  |hasData| guard is necessary here.
  if (initializer->hasData()) {
    v8::Local<v8::Value> data = initializer->data().V8Value();
    // TODO(crbug.com/1070871): Remove the following IsNullOrUndefined() check.
    // This null/undefined check fills the gap between the new and old bindings
    // code.  The new behavior is preferred in a long term, and we'll switch to
    // the new behavior once the migration to the new bindings gets settled.
    if (!data->IsNullOrUndefined()) {
      data_as_v8_value_.Set(initializer->data().GetIsolate(), data);
    }
  }
  if (initializer->hasOrigin()) {
    potentially_invalid_origin_serialization_ = initializer->origin();
    origin_ = SecurityOrigin::CreateFromString(initializer->origin());
  }
  if (initializer->hasLastEventId())
    last_event_id_ = initializer->lastEventId();
  if (initializer->hasSource() && IsValidSource(initializer->source()))
    source_ = initializer->source();
  if (initializer->hasPorts())
    ports_ = MakeGarbageCollected<GCedMessagePortArray>(initializer->ports());
  if (initializer->hasUserActivation())
    user_activation_ = initializer->userActivation();
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(scoped_refptr<const SecurityOrigin> origin,
                           const String& last_event_id,
                           EventTarget* source,
                           GCedMessagePortArray* ports)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeScriptValue),
      origin_(std::move(origin)),
      last_event_id_(last_event_id),
      source_(source),
      ports_(ports) {
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(scoped_refptr<SerializedScriptValue> data,
                           scoped_refptr<const SecurityOrigin> origin,
                           MessageOriginKind message_origin_kind,
                           const String& last_event_id,
                           EventTarget* source,
                           GCedMessagePortArray* ports,
                           UserActivation* user_activation)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeSerializedScriptValue),
      data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(data))),
      data_is_from_untrusted_source_(message_origin_kind ==
                                     kMessageIsCrossOrigin),
      origin_(std::move(origin)),
      last_event_id_(last_event_id),
      source_(source),
      ports_(ports),
      user_activation_(user_activation) {
  DCHECK(IsValidSource(source_.Get()));
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

MessageEvent::MessageEvent(
    scoped_refptr<SerializedScriptValue> data,
    scoped_refptr<const SecurityOrigin> origin,
    MessageOriginKind message_origin_kind,
    const String& last_event_id,
    EventTarget* source,
    Vector<MessagePortChannel> channels,
    UserActivation* user_activation,
    mojom::blink::DelegatedCapability delegated_capability)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeSerializedScriptValue),
      data_as_serialized_script_value_(
          SerializedScriptValue::Unpack(std::move(data))),
      data_is_from_untrusted_source_(message_origin_kind ==
                                     kMessageIsCrossOrigin),
      origin_(std::move(origin)),
      last_event_id_(last_event_id),
      source_(source),
      channels_(std::move(channels)),
      user_activation_(user_activation),
      delegated_capability_(delegated_capability) {
  DCHECK(IsValidSource(source_.Get()));
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

MessageEvent::MessageEvent(scoped_refptr<const SecurityOrigin> origin,
                           EventTarget* source)
    : Event(event_type_names::kMessageerror, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeNull),
      origin_(std::move(origin)),
      source_(source) {
  DCHECK(IsValidSource(source_.Get()));
}

MessageEvent::MessageEvent(const String& data,
                           scoped_refptr<const SecurityOrigin> origin)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeString),
      data_as_string_(data),
      origin_(std::move(origin)) {
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

MessageEvent::MessageEvent(Blob* data,
                           scoped_refptr<const SecurityOrigin> origin)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeBlob),
      data_as_blob_(data),
      origin_(std::move(origin)) {
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

MessageEvent::MessageEvent(DOMArrayBuffer* data,
                           scoped_refptr<const SecurityOrigin> origin)
    : Event(event_type_names::kMessage, Bubbles::kNo, Cancelable::kNo),
      data_type_(kDataTypeArrayBuffer),
      data_as_array_buffer_(data),
      origin_(std::move(origin)) {
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

MessageEvent::~MessageEvent() {
  serialized_data_memory_accounter_.Clear(v8::Isolate::GetCurrent());
}

MessageEvent* MessageEvent::Create(const AtomicString& type,
                                   const MessageEventInit* initializer,
                                   ExceptionState& exception_state) {
  if (initializer->source() && !IsValidSource(initializer->source())) {
    exception_state.ThrowTypeError(
        "The optional 'source' property is neither a Window nor MessagePort.");
    return nullptr;
  }
  return MakeGarbageCollected<MessageEvent>(type, initializer);
}

void MessageEvent::initMessageEvent(const AtomicString& type,
                                    bool bubbles,
                                    bool cancelable,
                                    const ScriptValue& data,
                                    const String& origin,
                                    const String& last_event_id,
                                    EventTarget* source,
                                    MessagePortArray ports) {
  if (IsBeingDispatched())
    return;

  initEvent(type, bubbles, cancelable);

  data_type_ = kDataTypeScriptValue;
  data_as_v8_value_.Set(data.GetIsolate(), data.V8Value());
  is_data_dirty_ = true;
  potentially_invalid_origin_serialization_ = origin;
  origin_ = SecurityOrigin::CreateFromString(origin);
  last_event_id_ = last_event_id;
  source_ = source;
  if (ports.empty()) {
    ports_ = nullptr;
  } else {
    ports_ = MakeGarbageCollected<GCedMessagePortArray>(std::move(ports));
  }
  is_ports_dirty_ = true;
}

void MessageEvent::initMessageEvent(
    const AtomicString& type,
    bool bubbles,
    bool cancelable,
    scoped_refptr<SerializedScriptValue> data,
    scoped_refptr<const SecurityOrigin> origin,
    MessageOriginKind message_origin_kind,
    const String& last_event_id,
    EventTarget* source,
    GCedMessagePortArray* ports,
    UserActivation* user_activation,
    mojom::blink::DelegatedCapability delegated_capability) {
  if (IsBeingDispatched())
    return;

  initEvent(type, bubbles, cancelable);

  data_type_ = kDataTypeSerializedScriptValue;
  data_as_serialized_script_value_ =
      SerializedScriptValue::Unpack(std::move(data));
  is_data_dirty_ = true;
  data_is_from_untrusted_source_ = message_origin_kind == kMessageIsCrossOrigin;
  origin_ = std::move(origin);
  last_event_id_ = last_event_id;
  source_ = source;
  ports_ = ports;
  is_ports_dirty_ = true;
  user_activation_ = user_activation;
  delegated_capability_ = delegated_capability;
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

void MessageEvent::initMessageEvent(const AtomicString& type,
                                    bool bubbles,
                                    bool cancelable,
                                    const String& data,
                                    scoped_refptr<const SecurityOrigin> origin,
                                    const String& last_event_id,
                                    EventTarget* source,
                                    GCedMessagePortArray* ports) {
  if (IsBeingDispatched())
    return;

  initEvent(type, bubbles, cancelable);

  data_type_ = kDataTypeString;
  data_as_string_ = data;
  is_data_dirty_ = true;
  origin_ = std::move(origin);
  last_event_id_ = last_event_id;
  source_ = source;
  ports_ = ports;
  is_ports_dirty_ = true;
  serialized_data_memory_accounter_.Increase(v8::Isolate::GetCurrent(),
                                             SizeOfExternalMemoryInBytes());
}

ScriptValue MessageEvent::data(ScriptState* script_state) {
  // Measure how often developers access `data` prior to accessing (and
  // hopefully evaluating!) `origin` as a way of evaluating the viability of
  // https://github.com/mikewest/incentivize-origin-checks/.
  if (should_measure_data_access_before_origin_) {
    if (ExecutionContext* context = ExecutionContext::From(script_state)) {
      const SecurityOrigin* receiving_origin = context->GetSecurityOrigin();
      if (!origin_ || origin_->IsOpaque()) {
        UseCounter::Count(context,
                          WebFeature::kMessageEventDataBeforeOpaqueOrigin);
      } else if (origin_->IsSameOriginWith(receiving_origin)) {
        UseCounter::Count(context,
                          WebFeature::kMessageEventDataBeforeSameOrigin);
      } else if (origin_->IsSameSiteWith(receiving_origin)) {
        UseCounter::Count(context,
                          WebFeature::kMessageEventDataBeforeSameSiteOrigin);
      } else {
        UseCounter::Count(context,
                          WebFeature::kMessageEventDataBeforeCrossSiteOrigin);
      }
    }
    should_measure_data_access_before_origin_ = false;
  }

  is_data_dirty_ = false;

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> value;
  switch (data_type_) {
    case kDataTypeNull:
      value = v8::Null(isolate);
      break;

    case kDataTypeScriptValue:
      if (data_as_v8_value_.IsEmpty())
        value = v8::Null(isolate);
      else
        value = data_as_v8_value_.GetAcrossWorld(script_state);
      break;

    case MessageEvent::kDataTypeSerializedScriptValue:
      if (data_as_serialized_script_value_) {
        static constexpr size_t kSlowDeserializationSizeThresholdBytes =
            16 * 1024;
        // The data is put on the V8 GC heap here, and therefore the V8 GC does
        // the accounting from here on. We unregister the registered memory to
        // avoid double accounting.
        serialized_data_memory_accounter_.Clear(isolate);
        MessagePortArray message_ports = ports();
        SerializedScriptValue::DeserializeOptions options;
        options.message_ports = &message_ports;
        options.slow_mode =
            RuntimeEnabledFeatures::
                MaskDeserializationTimeForCrossOriginMessagesEnabled() &&
            data_is_from_untrusted_source_ &&
            data_as_serialized_script_value_->Value()->DataLengthInBytes() >=
                kSlowDeserializationSizeThresholdBytes;
        value = data_as_serialized_script_value_->Deserialize(isolate, options);
      } else {
        value = v8::Null(isolate);
      }
      break;

    case MessageEvent::kDataTypeString:
      value = V8String(isolate, data_as_string_);
      break;

    case MessageEvent::kDataTypeBlob:
      value = ToV8Traits<Blob>::ToV8(script_state, data_as_blob_);
      break;

    case MessageEvent::kDataTypeArrayBuffer:
      value =
          ToV8Traits<DOMArrayBuffer>::ToV8(script_state, data_as_array_buffer_);
      break;
  }

  return ScriptValue(isolate, value);
}

String MessageEvent::originForBindings() {
  data_is_from_untrusted_source_ = false;
  should_measure_data_access_before_origin_ = false;
  if (!potentially_invalid_origin_serialization_.IsNull()) {
    return potentially_invalid_origin_serialization_;
  }

  // If no origin was provided (e.g. we're generating this event via
  // `MessagePort.postMessage`), then we'll serialize to the empty string.
  //
  // If a local-scheme origin was provided, serialize to `null`.
  //
  // TODO(40554285): The `file:` case should depend upon the
  // `allow_file_access_from_file_urls` preference, but that unfortunately
  // does not yet persist after round-tripping through `url::Origin`.
  // Serializing to `null` is consistent with our historical behavior, and
  // safe.
  if (!origin_) {
    return "";
  } else if (origin_->IsLocal()) {
    return "null";
  }
  return origin_->ToString();
}

const AtomicString& MessageEvent::InterfaceName() const {
  return event_interface_names::kMessageEvent;
}

MessagePortArray MessageEvent::ports() {
  // TODO(bashi): Currently we return a copied array because the binding
  // layer could modify the content of the array while executing JS callbacks.
  // Avoid copying once we can make sure that the binding layer won't
  // modify the content.
  is_ports_dirty_ = false;
  return ports_ ? MessagePortArray(*ports_) : MessagePortArray();
}

bool MessageEvent::IsOriginCheckRequiredToAccessData() const {
  if (data_type_ != kDataTypeSerializedScriptValue) {
    return false;
  }
  return data_as_serialized_script_value_->Value()->IsOriginCheckRequired();
}

bool MessageEvent::IsLockedToAgentCluster() const {
  if (locked_to_agent_cluster_)
    return true;
  if (data_type_ != kDataTypeSerializedScriptValue) {
    return false;
  }
  return data_as_serialized_script_value_->Value()->IsLockedToAgentCluster();
}

bool MessageEvent::CanDeserializeIn(ExecutionContext* execution_context) const {
  return data_type_ != kDataTypeSerializedScriptValue ||
         data_as_serialized_script_value_->Value()->CanDeserializeIn(
             execution_context);
}

void MessageEvent::EntangleMessagePorts(ExecutionContext* context) {
  ports_ = MessagePort::EntanglePorts(*context, std::move(channels_));
  is_ports_dirty_ = true;
}

void MessageEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_as_v8_value_);
  visitor->Trace(data_as_serialized_script_value_);
  visitor->Trace(data_as_blob_);
  visitor->Trace(data_as_array_buffer_);
  visitor->Trace(source_);
  visitor->Trace(ports_);
  visitor->Trace(user_activation_);
  Event::Trace(visitor);
}

void MessageEvent::LockToAgentCluster() {
  locked_to_agent_cluster_ = true;
}

}  // namespace blink
