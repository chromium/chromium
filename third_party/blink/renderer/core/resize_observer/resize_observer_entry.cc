// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"

namespace blink {

ResizeObserverEntry::ResizeObserverEntry(Element* target,
                                         const LayoutRect& content_rect)
    : target_(target) {
  content_rect_ = DOMRectReadOnly::FromFloatRect(FloatRect(
      FloatPoint(content_rect.Location()), FloatSize(content_rect.Size())));
}

void ResizeObserverEntry::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(content_rect_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
