// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inline_list_item.h"

#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"

namespace blink {

LayoutNGInlineListItem::LayoutNGInlineListItem(Element* element)
    : LayoutInline(element) {
  SetConsumesSubtreeChangeNotification();
  RegisterSubtreeChangeListenerOnDescendants(true);
  View()->AddLayoutListItem();
}

void LayoutNGInlineListItem::WillBeDestroyed() {
  NOT_DESTROYED();
  if (View()) {
    View()->RemoveLayoutListItem();
  }
  LayoutInline::WillBeDestroyed();
}

const char* LayoutNGInlineListItem::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGInlineListItem";
}

bool LayoutNGInlineListItem::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGInlineListItem || LayoutInline::IsOfType(type);
}

void LayoutNGInlineListItem::InsertedIntoTree() {
  LayoutInline::InsertedIntoTree();
  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

void LayoutNGInlineListItem::WillBeRemovedFromTree() {
  LayoutInline::WillBeRemovedFromTree();
  ListItemOrdinal::ItemInsertedOrRemoved(this);
}

LayoutObject* LayoutNGInlineListItem::Marker() const {
  NOT_DESTROYED();
  return To<Element>(GetNode())->PseudoElementLayoutObject(kPseudoIdMarker);
}

void LayoutNGInlineListItem::UpdateMarkerTextIfNeeded() {
  LayoutObject* marker = Marker();
  if (auto* list_marker = ListMarker::Get(marker)) {
    list_marker->UpdateMarkerTextIfNeeded(*marker);
  }
}

void LayoutNGInlineListItem::StyleDidChange(StyleDifference diff,
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

void LayoutNGInlineListItem::UpdateCounterStyle() {
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
}

int LayoutNGInlineListItem::Value() const {
  DCHECK(GetNode());
  return ordinal_.Value(*GetNode());
}

void LayoutNGInlineListItem::OrdinalValueChanged() {
  LayoutObject* marker = Marker();
  if (auto* list_marker = ListMarker::Get(marker)) {
    list_marker->OrdinalValueChanged(*marker);
    // UpdateMarkerTextIfNeeded() will be called by CollectInlinesInternal().
    marker->SetNeedsCollectInlines();
  }
}

void LayoutNGInlineListItem::SubtreeDidChange() {
  LayoutObject* marker = Marker();
  auto* list_marker = ListMarker::Get(marker);
  if (!list_marker) {
    return;
  }
  DCHECK(marker->IsLayoutNGInsideListMarker());
  list_marker->UpdateMarkerContentIfNeeded(*marker);
}

}  // namespace blink
