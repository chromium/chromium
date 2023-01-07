// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_plane_set.h"

namespace blink {

XRPlaneSet::XRPlaneSet(HeapHashSet<Member<XRPlane>> planes) : planes_(planes) {}

const HeapHashSet<Member<XRPlane>>& XRPlaneSet::elements() const {
  return planes_;
}

void XRPlaneSet::Trace(Visitor* visitor) const {
  visitor->Trace(planes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
