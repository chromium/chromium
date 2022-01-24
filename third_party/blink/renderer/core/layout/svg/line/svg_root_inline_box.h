/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_ROOT_INLINE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_ROOT_INLINE_BOX_H_

#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class SVGRootInlineBox final : public RootInlineBox {
 public:
  SVGRootInlineBox(LineLayoutItem block) : RootInlineBox(block) {}

  bool IsSVGRootInlineBox() const override { return true; }

  LayoutUnit VirtualLogicalHeight() const override { return logical_height_; }
  void SetLogicalHeight(LayoutUnit height) { logical_height_ = height; }

  void Paint(const PaintInfo&,
             const PhysicalOffset&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;

  void MarkDirty() override;

  void ComputePerCharacterLayoutInformation();

  InlineBox* ClosestLeafChildForPosition(const PhysicalOffset&);

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) final;

 private:
  void ReorderValueLists();
  gfx::RectF LayoutInlineBoxes(InlineBox&);

  LayoutUnit logical_height_;
};

template <>
struct DowncastTraits<SVGRootInlineBox> {
  static bool AllowFrom(const InlineBox& box) {
    return box.IsSVGRootInlineBox();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LINE_SVG_ROOT_INLINE_BOX_H_
