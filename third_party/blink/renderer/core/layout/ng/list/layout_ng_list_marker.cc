// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

namespace blink {

LayoutNGListMarker::LayoutNGListMarker(Element* element)
    : LayoutNGMixin<LayoutBlockFlow>(element) {}

LayoutNGListMarker* LayoutNGListMarker::CreateAnonymous(Document* document) {
  LayoutNGListMarker* object = new LayoutNGListMarker(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGListMarker::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListMarker ||
         LayoutNGMixin<LayoutBlockFlow>::IsOfType(type);
}

bool LayoutNGListMarker::IsListMarkerWrapperForBlockContent(
    const LayoutObject& object) {
  if (!object.IsAnonymous() || !object.IsLayoutBlockFlow())
    return false;
  const LayoutBlockFlow& block_flow = ToLayoutBlockFlow(object);
  if (const LayoutObject* child = block_flow.FirstChild()) {
    return child->IsLayoutNGListMarker() &&
           // The anonymous box should not have other children.
           // e.g., <li>text<div>block</div></li>
           // In this case, inline layout can handle the list marker.
           !child->NextSibling();
  }
  return false;
}

// The LayoutNGListItem this marker belongs to.
LayoutNGListItem* LayoutNGListMarker::ListItem() const {
  for (LayoutObject* parent = Parent(); parent; parent = parent->Parent()) {
    if (parent->IsLayoutNGListItem()) {
      DCHECK(ToLayoutNGListItem(parent)->Marker() == this);
      return ToLayoutNGListItem(parent);
    }
    // These DCHECKs are not critical but to ensure we cover all cases we know.
    DCHECK(parent->IsAnonymous());
    DCHECK(parent->IsLayoutBlockFlow() || parent->IsLayoutFlowThread());
  }
  return nullptr;
}

void LayoutNGListMarker::WillCollectInlines() {
  if (LayoutNGListItem* list_item = ListItem())
    list_item->UpdateMarkerTextIfNeeded();
}

bool LayoutNGListMarker::IsContentImage() const {
  return ListItem()->IsMarkerImage();
}

LayoutObject* LayoutNGListMarker::SymbolMarkerLayoutText() const {
  return ListItem()->SymbolMarkerLayoutText();
}

String LayoutNGListMarker::TextAlternative() const {
  // Compute from the list item, in the logical order even in RTL, reflecting
  // speech order.
  if (LayoutNGListItem* list_item = ListItem())
    return list_item->MarkerTextWithSuffix();
  return g_empty_string;
}

bool LayoutNGListMarker::NeedsOccupyWholeLine() const {
  if (!GetDocument().InQuirksMode())
    return false;

  LayoutObject* next_sibling = NextSibling();
  if (next_sibling && next_sibling->GetNode() &&
      (IsHTMLUListElement(*next_sibling->GetNode()) ||
       IsHTMLOListElement(*next_sibling->GetNode())))
    return true;

  return false;
}

}  // namespace blink
