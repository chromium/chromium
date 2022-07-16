// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_rect.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_init.h"

namespace blink {

DOMRect* DOMRect::Create(double x, double y, double width, double height) {
  return MakeGarbageCollected<DOMRect>(x, y, width, height);
}

DOMRect* DOMRect::FromFloatRect(const FloatRect& rect) {
  return MakeGarbageCollected<DOMRect>(rect.x(), rect.y(), rect.width(),
                                       rect.height());
}

DOMRect* DOMRect::fromRect(const DOMRectInit* other) {
  return MakeGarbageCollected<DOMRect>(other->x(), other->y(), other->width(),
                                       other->height());
}

DOMRect::DOMRect(double x, double y, double width, double height)
    : DOMRectReadOnly(x, y, width, height) {}

}  // namespace blink
