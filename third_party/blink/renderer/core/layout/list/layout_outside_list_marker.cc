// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/layout_outside_list_marker.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

LayoutOutsideListMarker::LayoutOutsideListMarker(Element* element)
    : LayoutBlockFlow(element) {}

void LayoutOutsideListMarker::WillCollectInlines() {
  list_marker_.UpdateMarkerTextIfNeeded(*this);
}

bool LayoutOutsideListMarker::IsMonolithic() const {
  return true;
}

bool LayoutOutsideListMarker::NeedsOccupyWholeLine() const {
  if (!GetDocument().InQuirksMode())
    return false;

  // Apply the quirks when the next sibling is a block-level `<ul>` or `<ol>`.
  LayoutObject* next_sibling = NextSibling();
  if (next_sibling && !next_sibling->IsInline() &&
      !next_sibling->IsFloatingOrOutOfFlowPositioned() &&
      next_sibling->GetNode() &&
      (IsA<HTMLUListElement>(*next_sibling->GetNode()) ||
       IsA<HTMLOListElement>(*next_sibling->GetNode())))
    return true;

  return false;
}

PositionWithAffinity LayoutOutsideListMarker::PositionForPoint(
    const PhysicalOffset&) const {
  DCHECK_GE(GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);
  return PositionBeforeThis();
}

}  // namespace blink
