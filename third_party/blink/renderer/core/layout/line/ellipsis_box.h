/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ELLIPSIS_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ELLIPSIS_BOX_H_

#include "third_party/blink/renderer/core/layout/line/inline_box.h"

namespace blink {

class HitTestResult;

class EllipsisBox final : public InlineBox {
 public:
  EllipsisBox(LineLayoutItem item,
              const AtomicString& ellipsis_str,
              InlineFlowBox* parent,
              LayoutUnit width,
              LayoutUnit height,
              LayoutPoint location,
              bool first_line,
              bool is_vertical)
      : InlineBox(item,
                  location,
                  width,
                  first_line,
                  true,
                  false,
                  false,
                  is_vertical,
                  nullptr,
                  nullptr,
                  parent),
        height_(height),
        str_(ellipsis_str) {
    SetHasVirtualLogicalHeight();
  }

  void Paint(const PaintInfo&,
             const PhysicalOffset&,
             LayoutUnit line_top,
             LayoutUnit line_bottom) const override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   LayoutUnit line_top,
                   LayoutUnit line_bottom) override;
  IntRect SelectionRect() const;

  LayoutUnit VirtualLogicalHeight() const override { return height_; }
  const AtomicString& EllipsisStr() const { return str_; }

  const char* BoxName() const override;

 private:
  LayoutUnit height_;
  AtomicString str_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_ELLIPSIS_BOX_H_
