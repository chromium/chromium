// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/handwriting/handwriting_stroke.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_handwriting_point.h"

namespace blink {

HandwritingStroke::HandwritingStroke() = default;

HandwritingStroke::~HandwritingStroke() = default;

// static
HandwritingStroke* HandwritingStroke::Create() {
  return MakeGarbageCollected<HandwritingStroke>();
}

void HandwritingStroke::addPoint(const HandwritingPoint* point) {
  points_.push_back(point);
}

const HeapVector<Member<const HandwritingPoint>>& HandwritingStroke::getPoints()
    const {
  return points_;
}

void HandwritingStroke::clear() {
  points_.clear();
}

void HandwritingStroke::Trace(Visitor* visitor) const {
  visitor->Trace(points_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
