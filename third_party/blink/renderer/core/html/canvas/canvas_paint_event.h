// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_PAINT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_PAINT_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_paint_event_init.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class CORE_EXPORT CanvasPaintEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CanvasPaintEvent* Create(const AtomicString& type,
                                  const CanvasPaintEventInit* initializer) {
    return MakeGarbageCollected<CanvasPaintEvent>(type, initializer);
  }

  CanvasPaintEvent(const AtomicString& type,
                   const CanvasPaintEventInit* initializer);
  ~CanvasPaintEvent() override;

  const HeapVector<Member<Element>>& changedElements() const;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<Element>> changed_elements_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_PAINT_EVENT_H_
