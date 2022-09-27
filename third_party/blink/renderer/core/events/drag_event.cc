// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/drag_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_drag_event_init.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"

namespace blink {

DragEvent::DragEvent() : data_transfer_(nullptr) {}

DragEvent::DragEvent(const AtomicString& type,
                     const DragEventInit* initializer,
                     base::TimeTicks platform_time_stamp,
                     SyntheticEventType synthetic_event_type)
    : MouseEvent(type, initializer, platform_time_stamp, synthetic_event_type),
      data_transfer_(initializer->getDataTransfer()) {}

bool DragEvent::IsDragEvent() const {
  return true;
}

bool DragEvent::IsMouseEvent() const {
  return false;
}

void DragEvent::Trace(Visitor* visitor) const {
  visitor->Trace(data_transfer_);
  MouseEvent::Trace(visitor);
}

DispatchEventResult DragEvent::DispatchEvent(EventDispatcher& dispatcher) {
  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());
  return dispatcher.Dispatch();
}

}  // namespace blink
