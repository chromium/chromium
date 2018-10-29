// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/list_item_ordinal.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// A LayoutObject subclass for 'display: list-item' in LayoutNG.
class CORE_EXPORT LayoutNGListItem final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGListItem(Element*);

  ListItemOrdinal& Ordinal() { return ordinal_; }

  int Value() const;
  String MarkerTextWithSuffix() const;
  String MarkerTextWithoutSuffix() const;

  LayoutObject* Marker() const { return marker_; }
  bool IsMarkerImage() const {
    return StyleRef().ListStyleImage() &&
           !StyleRef().ListStyleImage()->ErrorOccurred();
  }

  void UpdateMarkerTextIfNeeded() {
    if (marker_ && !is_marker_text_updated_ && !IsMarkerImage())
      UpdateMarkerText();
  }
  void UpdateMarkerContentIfNeeded();

  void OrdinalValueChanged();
  void WillCollectInlines() override;

  LayoutObject* SymbolMarkerLayoutText() const;
  static const LayoutObject* FindSymbolMarkerLayoutText(const LayoutObject*);

  const char* GetName() const override { return "LayoutNGListItem"; }

 private:
  bool IsOfType(LayoutObjectType) const override;

  void WillBeDestroyed() override;
  void InsertedIntoTree() override;
  void WillBeRemovedFromTree() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void SubtreeDidChange() final;

  bool IsInside() const;

  enum MarkerTextFormat { kWithSuffix, kWithoutSuffix };
  enum MarkerType { kStatic, kOrdinalValue, kSymbolValue };
  MarkerType MarkerText(StringBuilder*, MarkerTextFormat) const;
  void UpdateMarkerText();
  void UpdateMarkerText(LayoutText*);
  void UpdateMarker();
  void DestroyMarker();

  ListItemOrdinal ordinal_;
  LayoutObject* marker_ = nullptr;

  unsigned marker_type_ : 2;  // MarkerType
  unsigned is_marker_text_updated_ : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutNGListItem, IsLayoutNGListItem());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LIST_LAYOUT_NG_LIST_ITEM_H_
