// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_GEOMETRY_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_GEOMETRY_CHANGE_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class DOMRect;
class WindowControlsOverlayGeometryChangeEventInit;

class WindowControlsOverlayGeometryChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WindowControlsOverlayGeometryChangeEvent* Create(
      const AtomicString& type,
      const WindowControlsOverlayGeometryChangeEventInit*);

  WindowControlsOverlayGeometryChangeEvent(
      const AtomicString& type,
      const WindowControlsOverlayGeometryChangeEventInit*);
  WindowControlsOverlayGeometryChangeEvent(const AtomicString& type,
                                           DOMRect* rect,
                                           bool visible);

  DOMRect* titlebarAreaRect() const;
  bool visible() const;

  void Trace(Visitor*) const override;

 private:
  Member<DOMRect> bounding_rect_;
  bool visible_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_GEOMETRY_CHANGE_EVENT_H_
