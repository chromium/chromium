// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "before_create_policy_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

BeforeCreatePolicyEvent* BeforeCreatePolicyEvent::Create(
    const String& policy_name) {
  return MakeGarbageCollected<BeforeCreatePolicyEvent>(policy_name);
}

BeforeCreatePolicyEvent::BeforeCreatePolicyEvent(const String& policy_name)
    : Event(event_type_names::kBeforecreatepolicy,
            Bubbles::kNo,
            Cancelable::kYes),
      policy_name_(policy_name) {}

BeforeCreatePolicyEvent::~BeforeCreatePolicyEvent() = default;

bool BeforeCreatePolicyEvent::IsBeforeCreatePolicyEvent() const {
  return true;
}

const AtomicString& BeforeCreatePolicyEvent::InterfaceName() const {
  return event_interface_names::kBeforeCreatePolicyEvent;
}

void BeforeCreatePolicyEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
