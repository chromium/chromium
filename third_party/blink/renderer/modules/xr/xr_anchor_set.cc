// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_anchor_set.h"

namespace blink {

XRAnchorSet::XRAnchorSet(HeapHashSet<Member<XRAnchor>> anchors)
    : anchors_(anchors) {}

const HeapHashSet<Member<XRAnchor>>& XRAnchorSet::elements() const {
  return anchors_;
}

void XRAnchorSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(anchors_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
