// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_ENTRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Element;
class DOMRectReadOnly;
class ResizeObserverSize;

class CORE_EXPORT ResizeObserverEntry final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ResizeObserverEntry(Element* target);

  Element* target() const { return target_; }
  DOMRectReadOnly* contentRect() const { return content_rect_; }
  HeapVector<Member<ResizeObserverSize>> contentBoxSize() const {
    return content_box_size_;
  }
  HeapVector<Member<ResizeObserverSize>> borderBoxSize() const {
    return border_box_size_;
  }
  HeapVector<Member<ResizeObserverSize>> devicePixelContentBoxSize() const {
    return device_pixel_content_box_size_;
  }

  void Trace(Visitor*) const override;

 private:
  Member<Element> target_;
  Member<DOMRectReadOnly> content_rect_;
  HeapVector<Member<ResizeObserverSize>> device_pixel_content_box_size_;
  HeapVector<Member<ResizeObserverSize>> content_box_size_;
  HeapVector<Member<ResizeObserverSize>> border_box_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_ENTRY_H_
