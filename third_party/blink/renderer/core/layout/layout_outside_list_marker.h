// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OUTSIDE_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OUTSIDE_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"

namespace blink {

// Used to layout the list item's outside marker.
// The LayoutOutsideListMarker always has to be a child of a LayoutListItem.
class CORE_EXPORT LayoutOutsideListMarker final : public LayoutBlockFlow {
 public:
  explicit LayoutOutsideListMarker(Element*);
  ~LayoutOutsideListMarker() override;

  const char* GetName() const override { return "LayoutOutsideListMarker"; }

  bool IsMarkerImage() const;
  LayoutUnit ListItemInlineStartOffset() const {
    return list_item_inline_start_offset_;
  }
  void UpdateMargins();

  const ListMarker& Marker() const { return list_marker_; }
  ListMarker& Marker() { return list_marker_; }

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectOutsideListMarker ||
           LayoutBlockFlow::IsOfType(type);
  }

  void UpdateLayout() override;

  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  LayoutUnit list_item_inline_start_offset_;
  ListMarker list_marker_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutOutsideListMarker,
                                IsOutsideListMarkerForCustomContent());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OUTSIDE_LIST_MARKER_H_
