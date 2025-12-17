// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/extendable_message_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_client_messageport_serviceworker.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/url/dom_origin.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"

namespace blink {

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer) {
  return MakeGarbageCollected<ExtendableMessageEvent>(type, initializer);
}

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    scoped_refptr<SerializedScriptValue> data,
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    ServiceWorkerClient* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(data), std::move(origin), ports, observer);
  event->source_as_client_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::Create(
    scoped_refptr<SerializedScriptValue> data,
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    ServiceWorker* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(data), std::move(origin), ports, observer);
  event->source_as_service_worker_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::CreateError(
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    ServiceWorkerClient* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(origin), ports, observer);
  event->source_as_client_ = source;
  return event;
}

ExtendableMessageEvent* ExtendableMessageEvent::CreateError(
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    ServiceWorker* source,
    WaitUntilObserver* observer) {
  ExtendableMessageEvent* event = MakeGarbageCollected<ExtendableMessageEvent>(
      std::move(origin), ports, observer);
  event->source_as_service_worker_ = source;
  return event;
}

ScriptValue ExtendableMessageEvent::data(ScriptState* script_state) const {
  v8::Local<v8::Value> value;
  if (!data_.IsEmpty()) {
    value = data_.GetAcrossWorld(script_state);
  } else if (serialized_data_) {
    SerializedScriptValue::DeserializeOptions options;
    MessagePortArray message_ports = ports();
    options.message_ports = &message_ports;
    value = serialized_data_->Deserialize(script_state->GetIsolate(), options);
  } else {
    value = v8::Null(script_state->GetIsolate());
  }
  return ScriptValue(script_state->GetIsolate(), value);
}

V8UnionClientOrMessagePortOrServiceWorker* ExtendableMessageEvent::source()
    const {
  if (source_as_client_) {
    return MakeGarbageCollected<V8UnionClientOrMessagePortOrServiceWorker>(
        source_as_client_);
  } else if (source_as_service_worker_) {
    return MakeGarbageCollected<V8UnionClientOrMessagePortOrServiceWorker>(
        source_as_service_worker_);
  } else if (source_as_message_port_) {
    return MakeGarbageCollected<V8UnionClientOrMessagePortOrServiceWorker>(
        source_as_message_port_);
  }
  return nullptr;
}

MessagePortArray ExtendableMessageEvent::ports() const {
  // TODO(bashi): Currently we return a copied array because the binding
  // layer could modify the content of the array while executing JS callbacks.
  // Avoid copying once we can make sure that the binding layer won't
  // modify the content.
  if (ports_) {
    return MessagePortArray(*ports_.Get());
  }
  return MessagePortArray();
}

String ExtendableMessageEvent::origin() const {
  if (!potentially_invalid_origin_serialization_.IsNull()) {
    return potentially_invalid_origin_serialization_;
  }
  return origin_ ? origin_->ToString() : String();
}

const AtomicString& ExtendableMessageEvent::InterfaceName() const {
  return event_interface_names::kExtendableMessageEvent;
}

DOMOrigin* ExtendableMessageEvent::GetDOMOrigin(LocalDOMWindow*) const {
  // We only create `DOMOrigin` objects for `ExtendableMessageEvent` objects
  // that were not constructed from JavaScript, as the JavaScript constructor
  // accepts an untrusted string serialization of an origin.
  if (!potentially_invalid_origin_serialization_.IsNull() ||
      !GetSecurityOrigin()) {
    return nullptr;
  }

  // No access check is required, as this object intentionally reveals its
  // sender's origin cross-origin.
  return DOMOrigin::Create(GetSecurityOrigin());
}

void ExtendableMessageEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  visitor->Trace(source_as_client_);
  visitor->Trace(source_as_service_worker_);
  visitor->Trace(source_as_message_port_);
  visitor->Trace(ports_);
  ExtendableEvent::Trace(visitor);
}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer)
    : ExtendableMessageEvent(type, initializer, nullptr) {}

ExtendableMessageEvent::ExtendableMessageEvent(
    const AtomicString& type,
    const ExtendableMessageEventInit* initializer,
    WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer) {
  if (initializer->hasData()) {
    const ScriptValue& data = initializer->data();
    data_.Set(data.GetIsolate(), data.V8Value());
  }
  if (initializer->hasOrigin()) {
    potentially_invalid_origin_serialization_ = initializer->origin();
    origin_ = SecurityOrigin::CreateFromString(initializer->origin());
  }
  if (initializer->hasLastEventId())
    last_event_id_ = initializer->lastEventId();
  if (initializer->hasSource() and initializer->source()) {
    switch (initializer->source()->GetContentType()) {
      case V8UnionClientOrMessagePortOrServiceWorker::ContentType::kClient:
        source_as_client_ = initializer->source()->GetAsClient();
        break;
      case V8UnionClientOrMessagePortOrServiceWorker::ContentType::kMessagePort:
        source_as_message_port_ = initializer->source()->GetAsMessagePort();
        break;
      case V8UnionClientOrMessagePortOrServiceWorker::ContentType::
          kServiceWorker:
        source_as_service_worker_ = initializer->source()->GetAsServiceWorker();
        break;
    }
  }
  if (initializer->hasPorts())
    ports_ = MakeGarbageCollected<GCedMessagePortArray>(initializer->ports());
}

ExtendableMessageEvent::ExtendableMessageEvent(
    scoped_refptr<SerializedScriptValue> data,
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    WaitUntilObserver* observer)
    : ExtendableEvent(event_type_names::kMessage,
                      ExtendableMessageEventInit::Create(),
                      observer),
      serialized_data_(std::move(data)),
      origin_(std::move(origin)),
      last_event_id_(String()),
      ports_(ports) {
  if (serialized_data_)
    serialized_data_->RegisterMemoryAllocatedWithCurrentScriptContext();
}

ExtendableMessageEvent::ExtendableMessageEvent(
    scoped_refptr<const SecurityOrigin> origin,
    GCedMessagePortArray* ports,
    WaitUntilObserver* observer)
    : ExtendableEvent(event_type_names::kMessageerror,
                      ExtendableMessageEventInit::Create(),
                      observer),
      origin_(std::move(origin)),
      last_event_id_(String()),
      ports_(ports) {}

}  // namespace blink
