// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

LayoutNGInsideListMarker::LayoutNGInsideListMarker(Element* element)
    : LayoutInline(element) {}

bool LayoutNGInsideListMarker::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGInsideListMarker ||
         LayoutInline::IsOfType(type);
}

PositionWithAffinity LayoutNGInsideListMarker::PositionForPoint(
    const PhysicalOffset&) const {
  NOT_DESTROYED();
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  return PositionBeforeThis();
}

}  // namespace blink
