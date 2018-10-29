// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_connection_list.h"

#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_available_event.h"

namespace blink {

PresentationConnectionList::PresentationConnectionList(
    ExecutionContext* context)
    : ContextClient(context) {}

const AtomicString& PresentationConnectionList::InterfaceName() const {
  return EventTargetNames::PresentationConnectionList;
}

const HeapVector<Member<ReceiverPresentationConnection>>&
PresentationConnectionList::connections() const {
  return connections_;
}

void PresentationConnectionList::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == EventTypeNames::connectionavailable) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationRequestConnectionAvailableEventListener);
  }
}

void PresentationConnectionList::AddConnection(
    ReceiverPresentationConnection* connection) {
  connections_.push_back(connection);
}

bool PresentationConnectionList::RemoveConnection(
    ReceiverPresentationConnection* connection) {
  for (wtf_size_t i = 0; i < connections_.size(); i++) {
    if (connections_[i] == connection) {
      connections_.EraseAt(i);
      return true;
    }
  }
  return false;
}

void PresentationConnectionList::DispatchConnectionAvailableEvent(
    PresentationConnection* connection) {
  DispatchEvent(*PresentationConnectionAvailableEvent::Create(
      EventTypeNames::connectionavailable, connection));
}

bool PresentationConnectionList::IsEmpty() {
  return connections_.IsEmpty();
}

void PresentationConnectionList::Trace(blink::Visitor* visitor) {
  visitor->Trace(connections_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
