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

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class MessageEventInit;
class UserActivation;

class CORE_EXPORT MessageEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MessageEvent* Create() { return MakeGarbageCollected<MessageEvent>(); }
  static MessageEvent* Create(MessagePortArray* ports,
                              const String& origin = String(),
                              const String& last_event_id = String(),
                              EventTarget* source = nullptr) {
    return MakeGarbageCollected<MessageEvent>(origin, last_event_id, source,
                                              ports);
  }
  static MessageEvent* Create(MessagePortArray* ports,
                              scoped_refptr<SerializedScriptValue> data,
                              const String& origin = String(),
                              const String& last_event_id = String(),
                              EventTarget* source = nullptr) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), origin, last_event_id, source, ports, nullptr);
  }
  static MessageEvent* Create(MessagePortArray* ports,
                              scoped_refptr<SerializedScriptValue> data,
                              UserActivation* user_activation) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), String(), String(), nullptr, ports, user_activation);
  }
  static MessageEvent* Create(Vector<MessagePortChannel> channels,
                              scoped_refptr<SerializedScriptValue> data,
                              const String& origin = String(),
                              const String& last_event_id = String(),
                              EventTarget* source = nullptr,
                              UserActivation* user_activation = nullptr) {
    return MakeGarbageCollected<MessageEvent>(
        std::move(data), origin, last_event_id, source, std::move(channels),
        user_activation);
  }
  static MessageEvent* CreateError(const String& origin = String(),
                                   EventTarget* source = nullptr) {
    return MakeGarbageCollected<MessageEvent>(origin, source);
  }
  static MessageEvent* Create(const String& data,
                              const String& origin = String()) {
    return MakeGarbageCollected<MessageEvent>(data, origin);
  }
  static MessageEvent* Create(Blob* data, const String& origin = String()) {
    return MakeGarbageCollected<MessageEvent>(data, origin);
  }
  static MessageEvent* Create(DOMArrayBuffer* data,
                              const String& origin = String()) {
    return MakeGarbageCollected<MessageEvent>(data, origin);
  }
  static MessageEvent* Create(const AtomicString& type,
                              const MessageEventInit* initializer,
                              ExceptionState&);

  MessageEvent();
  MessageEvent(const AtomicString&, const MessageEventInit*);
  MessageEvent(const String& origin,
               const String& last_event_id,
               EventTarget* source,
               MessagePortArray*);
  MessageEvent(scoped_refptr<SerializedScriptValue> data,
               const String& origin,
               const String& last_event_id,
               EventTarget* source,
               MessagePortArray*,
               UserActivation* user_activation);
  MessageEvent(scoped_refptr<SerializedScriptValue> data,
               const String& origin,
               const String& last_event_id,
               EventTarget* source,
               Vector<MessagePortChannel>,
               UserActivation* user_activation);
  // Creates a "messageerror" event.
  MessageEvent(const String& origin, EventTarget* source);
  MessageEvent(const String& data, const String& origin);
  MessageEvent(Blob* data, const String& origin);
  MessageEvent(DOMArrayBuffer* data, const String& origin);
  ~MessageEvent() override;

  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        const ScriptValue& data,
                        const String& origin,
                        const String& last_event_id,
                        EventTarget* source,
                        MessagePortArray& ports);
  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        scoped_refptr<SerializedScriptValue> data,
                        const String& origin,
                        const String& last_event_id,
                        EventTarget* source,
                        MessagePortArray*,
                        UserActivation* user_activation);
  void initMessageEvent(const AtomicString& type,
                        bool bubbles,
                        bool cancelable,
                        const String& data,
                        const String& origin,
                        const String& last_event_id,
                        EventTarget* source,
                        MessagePortArray*);

  ScriptValue data(ScriptState*);
  bool IsDataDirty() const { return is_data_dirty_; }
  const String& origin() const { return origin_; }
  const String& lastEventId() const { return last_event_id_; }
  EventTarget* source() const { return source_.Get(); }
  MessagePortArray ports();
  bool isPortsDirty() const { return is_ports_dirty_; }
  UserActivation* userActivation() const { return user_activation_; }

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

  void EntangleMessagePorts(ExecutionContext*);

  void Trace(Visitor*) const override;

  void LockToAgentCluster();

  WARN_UNUSED_RESULT v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

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

  void RegisterAmountOfExternallyAllocatedMemory();

  void UnregisterAmountOfExternallyAllocatedMemory();

  DataType data_type_;
  WorldSafeV8Reference<v8::Value> data_as_v8_value_;
  Member<UnpackedSerializedScriptValue> data_as_serialized_script_value_;
  String data_as_string_;
  Member<Blob> data_as_blob_;
  Member<DOMArrayBuffer> data_as_array_buffer_;
  bool is_data_dirty_ = true;
  String origin_;
  String last_event_id_;
  Member<EventTarget> source_;
  // ports_ are the MessagePorts in an entangled state, and channels_ are
  // the MessageChannels in a disentangled state. Only one of them can be
  // non-empty at a time. EntangleMessagePorts() moves between the states.
  Member<MessagePortArray> ports_;
  bool is_ports_dirty_ = true;
  Vector<MessagePortChannel> channels_;
  Member<UserActivation> user_activation_;
  size_t amount_of_external_memory_ = 0;
  // For serialized messages across process this attribute contains the
  // information of whether the actual original SerializedScriptValue was locked
  // to the agent cluster.
  bool locked_to_agent_cluster_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EVENTS_MESSAGE_EVENT_H_
