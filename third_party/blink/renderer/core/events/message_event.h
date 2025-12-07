/*
 * Copyright (C) 2007 Henry Mason (hmason@mac.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_MESSAGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_MESSAGE_EVENT_H_

#include <memory>

#include "base/check_op.h"
#include "third_party/blink/public/mojom/messaging/delegated_capability.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/url/dom_origin_utils.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class MessageEventInit;
class UserActivation;

class CORE_EXPORT MessageEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum MessageOriginKind {
    kMessageIsSameOrigin,
    kMessageIsCrossOrigin,
  };

  static MessageEvent* Create() { return MakeGarbageCollected<MessageEvent>(); }
  static MessageEvent* Create(GCedMessagePortArray* ports,
                              scoped_refptr<const SecurityOrigin> origin,
                              const String& last_event_id,
                              EventTarget* source) {
    return MakeGarbageCollected<MessageEvent>(std::move(origin), last_event_id,
                                              source, ports);
  }
  static MessageEvent* Create(GCedMessagePortArray* ports,
                              scoped_refptr<SerializedScriptValue> data,
                              scoped_refptr<const SecurityOrigin> origin,
                              MessageOriginKind message_origin_kind,
                              const String& last_event_id,
                              EventTarget* source) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), std::move(origin), message_origin_kind, last_event_id,
        source, ports, nullptr);
  }
  static MessageEvent* Create(GCedMessagePortArray* ports,
                              scoped_refptr<SerializedScriptValue> data,
                              UserActivation* user_activation) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), /*origin=*/nullptr, kMessageIsSameOrigin, String(),
        nullptr, ports, user_activation);
  }
  static MessageEvent* Create(
      Vector<MessagePortChannel> channels,
      scoped_refptr<SerializedScriptValue> data,
      scoped_refptr<const SecurityOrigin> origin,
      MessageOriginKind message_origin_kind,
      const String& last_event_id,
      EventTarget* source,
      UserActivation* user_activation,
      mojom::blink::DelegatedCapability delegated_capability) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), std::move(origin), message_origin_kind, last_event_id,
        source, std::move(channels), user_activation, delegated_capability);
  }
  static MessageEvent* CreateError() {
    scoped_refptr<const SecurityOrigin> nullptr_origin;
    return MakeGarbageCollected<MessageEvent>(std::move(nullptr_origin),
                                              nullptr);
  }
  static MessageEvent* CreateError(const MessageEvent* event) {
    return MakeGarbageCollected<MessageEvent>(event->GetSecurityOrigin(),
                                              event->source());
  }
  static MessageEvent* CreateError(const SecurityOrigin* origin,
                                   EventTarget* source = nullptr) {
    return MakeGarbageCollected<MessageEvent>(origin, source);
  }
  static MessageEvent* Create(
      const String& data,
      scoped_refptr<const SecurityOrigin> origin = nullptr) {
    return MakeGarbageCollected<MessageEvent>(data, std::move(origin));
  }
  static MessageEvent* Create(
      Blob* data,
      scoped_refptr<const SecurityOrigin> origin = nullptr) {
    return MakeGarbageCollected<MessageEvent>(data, std::move(origin));
  }
  static MessageEvent* Create(
      DOMArrayBuffer* data,
      scoped_refptr<const SecurityOrigin> origin = nullptr) {
    return MakeGarbageCollected<MessageEvent>(data, std::move(origin));
  }
  static MessageEvent* Create(const AtomicString& type,
                              const MessageEventInit* initializer,
                              ExceptionState&);

  MessageEvent();
  MessageEvent(const AtomicString&, const MessageEventInit*);
  MessageEvent(scoped_refptr<const SecurityOrigin> origin,
               const String& last_event_id,
               EventTarget* source,
               GCedMessagePortArray*);
  MessageEvent(scoped_refptr<SerializedScriptValue> data,
               scoped_refptr<const SecurityOrigin> origin,
               MessageOriginKind message_origin_kind,
               const String& last_event_id,
               EventTarget* source,
               GCedMessagePortArray*,
               UserActivation* user_activation);
  MessageEvent(scoped_refptr<SerializedScriptValue> data,
               scoped_refptr<const SecurityOrigin> origin,
               MessageOriginKind message_origin_kind,
               const String& last_event_id,
               EventTarget* source,
               Vector<MessagePortChannel>,
               UserActivation* user_activation,
               mojom::blink::DelegatedCapability delegated_capability);
  // Creates a "messageerror" event.
  MessageEvent(scoped_refptr<const SecurityOrigin> origin, EventTarget* source);
  MessageEvent(const String& data, scoped_refptr<const SecurityOrigin> origin);
  MessageEvent(Blob* data, scoped_refptr<const SecurityOrigin> origin);
  MessageEvent(DOMArrayBuffer* data,
               scoped_refptr<const SecurityOrigin> origin);
  ~MessageEvent() override;

  // DOMOriginUtils overrides:
  DOMOrigin* GetDOMOrigin(LocalDOMWindow*) const override;

  // This is exposed to JavaScript, and so accepts a serialized |origin| rather
  // than a `SecurityOrigin`.
  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        const ScriptValue& data,
                        const String& origin,
                        const String& last_event_id,
                        EventTarget* source,
                        MessagePortArray ports);

  // These `initMessageEvent` overrides are not actually implementations of the
  // bindings-exposed `initMessageEvent` method, and should be renamed.
  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        scoped_refptr<SerializedScriptValue> data,
                        scoped_refptr<const SecurityOrigin> origin,
                        MessageOriginKind message_origin_kind,
                        const String& last_event_id,
                        EventTarget* source,
                        GCedMessagePortArray*,
                        UserActivation* user_activation,
                        mojom::blink::DelegatedCapability delegated_capability);
  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        const String& data,
                        scoped_refptr<const SecurityOrigin> origin,
                        const String& last_event_id,
                        EventTarget* source,
                        GCedMessagePortArray*);

  // To evaluate the viability of shipping anything remotely resembling
  // https://github.com/mikewest/incentivize-origin-checks/, this method should
  // be called when `MessageEvent` objects are sent to `Window` via
  // `postMessage()`.
  void SetShouldMeasureDataAccessBeforeOrigin() {
    should_measure_data_access_before_origin_ = true;
  }

  ScriptValue data(ScriptState*);
  bool IsDataDirty() const { return is_data_dirty_; }
  // This returns a serialized origin string (which might be "null") to support
  // JavaScript bindings. Prefer `GetSecurityOrigin()` below for other uses.
  String originForBindings();
  const String& lastEventId() const { return last_event_id_; }
  EventTarget* source() const { return source_.Get(); }
  MessagePortArray ports();
  bool isPortsDirty() const { return is_ports_dirty_; }
  UserActivation* userActivation() const { return user_activation_.Get(); }
  mojom::blink::DelegatedCapability delegatedCapability() const {
    return delegated_capability_;
  }
  uint64_t GetTraceId() const { return trace_id_; }
  void SetTraceId(uint64_t trace_id) { trace_id_ = trace_id; }

  Vector<MessagePortChannel> ReleaseChannels() { return std::move(channels_); }

  const AtomicString& InterfaceName() const override;

  // Use with caution. Since the data has already been unpacked, the underlying
  // SerializedScriptValue will no longer contain transferred contents.
  SerializedScriptValue* DataAsSerializedScriptValue() const {
    DCHECK_EQ(data_type_, kDataTypeSerializedScriptValue);
    return data_as_serialized_script_value_->Value();
  }

  // Returns true when |data_as_serialized_script_value_| contains values that
  // remote origins cannot access. If true, remote origins must dispatch a
  // messageerror event instead of message event.
  bool IsOriginCheckRequiredToAccessData() const;

  // Returns true when |data_as_serialized_script_value_| is locked to an
  // agent cluster.
  bool IsLockedToAgentCluster() const;

  // Returns true when |data_as_serialized_script_value_| is not prevented from
  // being deserialized in the provided execution context.
  bool CanDeserializeIn(ExecutionContext*) const;

  void EntangleMessagePorts(ExecutionContext*);

  void Trace(Visitor*) const override;

  void LockToAgentCluster();

  scoped_refptr<const SecurityOrigin> GetSecurityOrigin() const {
    return origin_;
  }

 private:
  enum DataType {
    kDataTypeNull,  // For "messageerror" events.
    kDataTypeScriptValue,
    kDataTypeSerializedScriptValue,
    kDataTypeString,
    kDataTypeBlob,
    kDataTypeArrayBuffer
  };

  size_t SizeOfExternalMemoryInBytes();

  DataType data_type_;
  WorldSafeV8Reference<v8::Value> data_as_v8_value_;
  Member<UnpackedSerializedScriptValue> data_as_serialized_script_value_;
  // Most data are, but in this particular case this refers to data coming
  // from a cross-origin source being accessed without caller having previously
  // accessed the origin property.
  bool data_is_from_untrusted_source_ = true;
  V8ExternalMemoryAccounter serialized_data_memory_accounter_;
  String data_as_string_;
  Member<Blob> data_as_blob_;
  Member<DOMArrayBuffer> data_as_array_buffer_;
  bool is_data_dirty_ = true;

  // We hold a `SecurityOrigin` in `origin_` which we'll use for any and all
  // security checks. We also potentially have to hold a string representing
  // the serialized origin that was handed to us if the `MessageEvent` was
  // constructed from JavaScript, as the object is specced to return that
  // string even if it's not a valid origin serialization. See
  // https://github.com/whatwg/html/issues/11759 for discussion.
  scoped_refptr<const SecurityOrigin> origin_;
  String potentially_invalid_origin_serialization_;

  bool should_measure_data_access_before_origin_ = false;
  String last_event_id_;
  Member<EventTarget> source_;
  // ports_ are the MessagePorts in an entangled state, and channels_ are
  // the MessageChannels in a disentangled state. Only one of them can be
  // non-empty at a time. EntangleMessagePorts() moves between the states.
  Member<GCedMessagePortArray> ports_;
  bool is_ports_dirty_ = true;
  Vector<MessagePortChannel> channels_;
  Member<UserActivation> user_activation_;
  mojom::blink::DelegatedCapability delegated_capability_;
  // For serialized messages across process this attribute contains the
  // information of whether the actual original SerializedScriptValue was locked
  // to the agent cluster.
  bool locked_to_agent_cluster_ = false;
  uint64_t trace_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_MESSAGE_EVENT_H_
