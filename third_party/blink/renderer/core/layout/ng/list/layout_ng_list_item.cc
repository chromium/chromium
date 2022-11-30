// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"

namespace blink {

LayoutNGListItem::LayoutNGListItem(Element* element)
    : LayoutNGBlockFlow(element) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
  View()->AddLayoutListItem();
}

void LayoutNGListItem::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View())
    View()->RemoveLayoutListItem();
  LayoutNGBlockFlow::WillBeDestroyed();
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

  if (old_style) {
    const ListStyleTypeData* old_list_style_type = old_style->ListStyleType();
    const ListStyleTypeData* new_list_style_type = StyleRef().ListStyleType();
    if (old_list_style_type != new_list_style_type &&
        (!old_list_style_type || !new_list_style_type ||
         *old_list_style_type != *new_list_style_type))
      list_marker->ListStyleTypeChanged(*marker);
  }
}

void LayoutNGListItem::UpdateCounterStyle() {
  if (!StyleRef().ListStyleType() ||
      StyleRef().ListStyleType()->IsCounterStyleReferenceValid(GetDocument())) {
    return;
  }

  LayoutObject* marker = Marker();
  ListMarker* list_marker = ListMarker::Get(marker);
  if (!list_marker)
    return;

  list_marker->CounterStyleChanged(*marker);
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
    return FindSymbolMarkerLayoutText(GetLayoutObjectForParentNode(object));

  if (object->IsLayoutNGTextCombine())
    return FindSymbolMarkerLayoutText(object->Parent());

  return nullptr;
}

}  // namespace blink
