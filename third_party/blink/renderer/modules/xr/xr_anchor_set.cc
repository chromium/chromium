// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor_set.h"

namespace blink {

XRAnchorSet::XRAnchorSet(HeapHashSet<Member<XRAnchor>> anchors)
    : anchors_(anchors) {}

const HeapHashSet<Member<XRAnchor>>& XRAnchorSet::elements() const {
  return anchors_;
}

void XRAnchorSet::Trace(Visitor* visitor) const {
  visitor->Trace(anchors_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
