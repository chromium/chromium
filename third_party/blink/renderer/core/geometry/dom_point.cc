// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/geometry/dom_point.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"

namespace blink {

DOMPoint* DOMPoint::Create(double x, double y, double z, double w) {
  return MakeGarbageCollected<DOMPoint>(x, y, z, w);
}

DOMPoint* DOMPoint::fromPoint(const DOMPointInit* other) {
  return MakeGarbageCollected<DOMPoint>(other->x(), other->y(), other->z(),
                                        other->w());
}

DOMPoint::DOMPoint(double x, double y, double z, double w)
    : DOMPointReadOnly(x, y, z, w) {}

}  // namespace blink
