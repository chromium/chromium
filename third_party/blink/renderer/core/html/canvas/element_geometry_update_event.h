// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_GEOMETRY_UPDATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_GEOMETRY_UPDATE_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_element_geometry_update_event_init.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class CORE_EXPORT ElementGeometryUpdateEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ElementGeometryUpdateEvent* Create(
      const AtomicString& type,
      const ElementGeometryUpdateEventInit* initializer) {
    return MakeGarbageCollected<ElementGeometryUpdateEvent>(type, initializer);
  }

  ElementGeometryUpdateEvent(const AtomicString& type,
                             const ElementGeometryUpdateEventInit* initializer);
  ~ElementGeometryUpdateEvent() override;

  const HeapVector<Member<Element>>& elements() const;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<Element>> elements_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_GEOMETRY_UPDATE_EVENT_H_
