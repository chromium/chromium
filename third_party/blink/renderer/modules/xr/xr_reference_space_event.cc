// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_reference_space_event.h"

#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

namespace blink {

XRReferenceSpaceEvent::XRReferenceSpaceEvent() = default;

XRReferenceSpaceEvent::XRReferenceSpaceEvent(const AtomicString& type,
                                             XRReferenceSpace* reference_space)
    : Event(type, Bubbles::kNo, Cancelable::kYes),
      reference_space_(reference_space) {}

XRReferenceSpaceEvent::XRReferenceSpaceEvent(
    const AtomicString& type,
    const XRReferenceSpaceEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasReferenceSpace())
    reference_space_ = initializer->referenceSpace();
  if (initializer->hasTransform())
    transform_ = initializer->transform();
}

XRReferenceSpaceEvent::~XRReferenceSpaceEvent() = default;

const AtomicString& XRReferenceSpaceEvent::InterfaceName() const {
  return event_interface_names::kXRReferenceSpaceEvent;
}

void XRReferenceSpaceEvent::Trace(Visitor* visitor) const {
  visitor->Trace(reference_space_);
  visitor->Trace(transform_);
  Event::Trace(visitor);
}

}  // namespace blink
