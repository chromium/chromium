/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009 Apple Inc.
 *               All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/list_marker.h"

namespace blink {

class LayoutListItem;

// Used to layout a list item's marker with 'content: normal'.
// The LayoutListMarker always has to be a child of a LayoutListItem.
class CORE_EXPORT LayoutListMarker final : public LayoutBox {
  friend class LayoutListItem;

 public:
  explicit LayoutListMarker(Element*);
  ~LayoutListMarker() override;

  // Marker text without suffix, e.g. "1".
  const String& GetText() const { return text_; }

  // Marker text with suffix, e.g. "1. ", for use in accessibility.
  String TextAlternative() const;

  ListMarker::ListStyleCategory GetListStyleCategory() const;

  bool IsInside() const;

  LayoutRect GetRelativeMarkerRect() const;

  bool IsImage() const override;
  const StyleImage* GetImage() const { return image_.Get(); }
  const LayoutListItem* ListItem() const;
  LayoutSize ImageBulletSize() const;

  const char* GetName() const override { return "LayoutListMarker"; }

  LayoutUnit ListItemInlineStartOffset() const {
    return list_item_inline_start_offset_;
  }

 protected:
  void WillBeDestroyed() override;

 private:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  MinMaxSizes PreferredLogicalWidths() const override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectListMarker || LayoutBox::IsOfType(type);
  }

  void Paint(const PaintInfo&) const override;

  void UpdateLayout() override;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

  InlineBox* CreateInlineBox() override;

  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  bool IsText() const { return !IsImage(); }

  LayoutUnit GetWidthOfText(ListMarker::ListStyleCategory) const;
  void UpdateMargins(LayoutUnit marker_inline_size);
  void UpdateMargins();
  void UpdateContent();

  void UpdateMarkerImageIfNeeded(StyleImage* image);
  void ListStyleTypeChanged();

  String text_;
  Persistent<StyleImage> image_;
  LayoutUnit list_item_inline_start_offset_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutListMarker,
                                IsListMarkerForNormalContent());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_LIST_MARKER_H_
