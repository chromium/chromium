// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

namespace blink {

LayoutNGListMarker::LayoutNGListMarker(Element* element)
    : LayoutNGBlockFlowMixin<LayoutBlockFlow>(element) {}

LayoutNGListMarker* LayoutNGListMarker::CreateAnonymous(Document* document) {
  LayoutNGListMarker* object = new LayoutNGListMarker(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGListMarker::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListMarker ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

void LayoutNGListMarker::WillCollectInlines() {
  if (LayoutNGListItem* list_item = LayoutNGListItem::FromMarker(*this))
    list_item->UpdateMarkerTextIfNeeded();
}

bool LayoutNGListMarker::IsContentImage() const {
  if (LayoutNGListItem* list_item = LayoutNGListItem::FromMarker(*this))
    return list_item->IsMarkerImage();
  return false;
}

LayoutObject* LayoutNGListMarker::SymbolMarkerLayoutText() const {
  if (LayoutNGListItem* list_item = LayoutNGListItem::FromMarker(*this))
    return list_item->SymbolMarkerLayoutText();
  return nullptr;
}

bool LayoutNGListMarker::NeedsOccupyWholeLine() const {
  if (!GetDocument().InQuirksMode())
    return false;

  LayoutObject* next_sibling = NextSibling();
  if (next_sibling && next_sibling->GetNode() &&
      (IsA<HTMLUListElement>(*next_sibling->GetNode()) ||
       IsA<HTMLOListElement>(*next_sibling->GetNode())))
    return true;

  return false;
}

PositionWithAffinity LayoutNGListMarker::PositionForPoint(
    const PhysicalOffset&) const {
  return CreatePositionWithAffinity(0);
}

}  // namespace blink
