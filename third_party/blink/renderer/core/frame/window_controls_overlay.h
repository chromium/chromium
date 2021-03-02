// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;

class CORE_EXPORT WindowControlsOverlay final
    : public EventTargetWithInlineData,
      public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  // Web Exposed as navigator.windowControlsOverlay
  static WindowControlsOverlay* windowControlsOverlay(Navigator& navigator);

  static WindowControlsOverlay& From(Navigator& navigator);
  static WindowControlsOverlay* FromIfExists(Navigator& navigator);

  explicit WindowControlsOverlay(Navigator& navigator);
  WindowControlsOverlay(const WindowControlsOverlay&) = delete;
  ~WindowControlsOverlay() override;

  WindowControlsOverlay& operator=(const WindowControlsOverlay&) = delete;

  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(geometrychange, kGeometrychange)

  bool visible() const;
  DOMRect* getBoundingClientRect() const;

  void WindowControlsOverlayChanged(const gfx::Rect& rect);

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WINDOW_CONTROLS_OVERLAY_H_
