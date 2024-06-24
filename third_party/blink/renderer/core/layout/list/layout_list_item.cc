// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"

namespace blink {

LayoutListItem::LayoutListItem(Element* element) : LayoutBlockFlow(element) {
  SetInline(false);

  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
  View()->AddLayoutListItem();
}

void LayoutListItem::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View())
    View()->RemoveLayoutListItem();
  LayoutBlockFlow::WillBeDestroyed();
}

void LayoutListItem::InsertedIntoTree() {
  LayoutBlockFlow::InsertedIntoTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutListItem::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutListItem::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  LayoutBlockFlow::StyleDidChange(diff, old_style);

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

void LayoutListItem::UpdateCounterStyle() {
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

void LayoutListItem::OrdinalValueChanged() {
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->OrdinalValueChanged(*marker);
}

void LayoutListItem::SubtreeDidChange() {
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker)) {
    list_marker->UpdateMarkerContentIfNeeded(*marker);
  }
}

void LayoutListItem::WillCollectInlines() {
  UpdateMarkerTextIfNeeded();
}

void LayoutListItem::UpdateMarkerTextIfNeeded() {
  LayoutObject* marker = Marker();
  if (ListMarker* list_marker = ListMarker::Get(marker))
    list_marker->UpdateMarkerTextIfNeeded(*marker);
}

int LayoutListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

const LayoutObject* LayoutListItem::FindSymbolMarkerLayoutText(
    const LayoutObject* object) {
  if (!object)
    return nullptr;

  if (const ListMarker* list_marker = ListMarker::Get(object))
    return list_marker->SymbolMarkerLayoutText(*object);

  if (object->IsLayoutListItem()) {
    return FindSymbolMarkerLayoutText(To<LayoutListItem>(object)->Marker());
  }

  if (const auto* inline_list_item = DynamicTo<LayoutInlineListItem>(object)) {
    return FindSymbolMarkerLayoutText(inline_list_item->Marker());
  }

  if (object->IsAnonymousBlock())
    return FindSymbolMarkerLayoutText(GetLayoutObjectForParentNode(object));

  if (object->IsLayoutTextCombine()) {
    return FindSymbolMarkerLayoutText(object->Parent());
  }

  return nullptr;
}

}  // namespace blink
