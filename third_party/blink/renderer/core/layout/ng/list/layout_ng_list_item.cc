// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

#include "third_party/blink/renderer/core/layout/list_marker.h"

namespace blink {

LayoutNGListItem::LayoutNGListItem(Element* element)
    : LayoutNGBlockFlow(element) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
}

bool LayoutNGListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListItem || LayoutNGBlockFlow::IsOfType(type);
}

void LayoutNGListItem::InsertedIntoTree() {
  LayoutNGBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::WillBeRemovedFromTree() {
  LayoutNGBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGListItem::StyleDidChange(StyleDifference diff,
                                      const ComputedStyle* old_style) {
  LayoutNGBlockFlow::StyleDidChange(diff, old_style);

  LayoutObject* marker = Marker();
  ListMarker* list_marker = ListMarker::Get(marker);
  if (!list_marker)
    return;

  list_marker->UpdateMarkerContentIfNeeded(*marker);

  if (old_style && (old_style->ListStyleType() != StyleRef().ListStyleType() ||
                    (StyleRef().ListStyleType() == EListStyleType::kString &&
                     old_style->ListStyleStringValue() !=
                         StyleRef().ListStyleStringValue())))
    list_marker->ListStyleTypeChanged(*marker);
}

void LayoutNGListItem::OrdinalValueChanged() {
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->OrdinalValueChanged(*marker);
}

void LayoutNGListItem::SubtreeDidChange() {
  LayoutObject* marker = Marker();
  ListMarker* list_marker = ListMarker::Get(marker);
  if (!list_marker)
    return;

  // Make sure an outside marker is a direct child of the list item (not nested
  // inside an anonymous box), and that a marker originated by a ::before or
  // ::after precedes the generated contents.
  if ((marker->IsLayoutNGOutsideListMarker() && marker->Parent() != this) ||
      (IsPseudoElement() && marker != FirstChild())) {
    marker->Remove();
    AddChild(marker, FirstChild());
  }

  list_marker->UpdateMarkerContentIfNeeded(*marker);
}

void LayoutNGListItem::WillCollectInlines() {
  UpdateMarkerTextIfNeeded();
}

void LayoutNGListItem::UpdateMarkerTextIfNeeded() {
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->UpdateMarkerTextIfNeeded(*marker);
}

int LayoutNGListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

const LayoutObject* LayoutNGListItem::FindSymbolMarkerLayoutText(
    const LayoutObject* object) {
  if (!object)
    return nullptr;

  if (const ListMarker* list_marker = ListMarker::Get(object))
    return list_marker->SymbolMarkerLayoutText(*object);

  if (object->IsLayoutNGListItem())
    return FindSymbolMarkerLayoutText(To<LayoutNGListItem>(object)->Marker());

  if (object->IsAnonymousBlock())
    return FindSymbolMarkerLayoutText(object->Parent());

  return nullptr;
}

}  // namespace blink
