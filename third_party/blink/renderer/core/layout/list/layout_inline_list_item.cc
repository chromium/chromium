// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/list/layout_inline_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"

namespace blink {

LayoutInlineListItem::LayoutInlineListItem(Element* element)
    : LayoutInline(element) {
  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
  View()->AddLayoutListItem();
}

void LayoutInlineListItem::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View()) {
    View()->RemoveLayoutListItem();
  }
  LayoutInline::WillBeDestroyed();
}

const char* LayoutInlineListItem::GetName() const {
  NOT_DESTROYED();
  return "LayoutInlineListItem";
}

void LayoutInlineListItem::InsertedIntoTree() {
  LayoutInline::InsertedIntoTree();
  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutInlineListItem::WillBeRemovedFromTree() {
  LayoutInline::WillBeRemovedFromTree();
  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

LayoutObject* LayoutInlineListItem::Marker() const {
  NOT_DESTROYED();
  return GetNode()->PseudoElementLayoutObject(kPseudoIdMarker);
}

void LayoutInlineListItem::UpdateMarkerTextIfNeeded() {
  LayoutObject* marker = Marker();
  if (auto* list_marker = ListMarker::Get(marker)) {
    list_marker->UpdateMarkerTextIfNeeded(*marker);
  }
}

void LayoutInlineListItem::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  LayoutInline::StyleDidChange(diff, old_style);

  LayoutObject* marker = Marker();
  auto* list_marker = ListMarker::Get(marker);
  if (!list_marker) {
    return;
  }
  list_marker->UpdateMarkerContentIfNeeded(*marker);

  if (old_style) {
    const ListStyleTypeData* old_list_style_type = old_style->ListStyleType();
    const ListStyleTypeData* new_list_style_type = StyleRef().ListStyleType();
    if (old_list_style_type != new_list_style_type &&
        (!old_list_style_type || !new_list_style_type ||
         *old_list_style_type != *new_list_style_type)) {
      list_marker->ListStyleTypeChanged(*marker);
      SetNeedsCollectInlines();
    }
  }
}

void LayoutInlineListItem::UpdateCounterStyle() {
  if (!StyleRef().ListStyleType() ||
      StyleRef().ListStyleType()->IsCounterStyleReferenceValid(GetDocument())) {
    return;
  }

  LayoutObject* marker = Marker();
  auto* list_marker = ListMarker::Get(marker);
  if (!list_marker) {
    return;
  }
  list_marker->CounterStyleChanged(*marker);
  SetNeedsCollectInlines();
}

int LayoutInlineListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

void LayoutInlineListItem::OrdinalValueChanged() {
  LayoutObject* marker = Marker();
  if (auto* list_marker = ListMarker::Get(marker)) {
    list_marker->OrdinalValueChanged(*marker);
    // UpdateMarkerTextIfNeeded() will be called by CollectInlinesInternal().
    marker->SetNeedsCollectInlines();
  }
}

void LayoutInlineListItem::SubtreeDidChange() {
  LayoutObject* marker = Marker();
  auto* list_marker = ListMarker::Get(marker);
  if (!list_marker) {
    return;
  }
  DCHECK(marker->IsLayoutInsideListMarker());
  list_marker->UpdateMarkerContentIfNeeded(*marker);
}

}  // namespace blink
