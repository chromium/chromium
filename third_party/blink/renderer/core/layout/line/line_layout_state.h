/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_LAYOUT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_LAYOUT_STATE_H_

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Like LayoutState for layout(), LineLayoutState keeps track of global
// information during an entire linebox tree layout pass (aka
// layoutInlineChildren).
class LineLayoutState {
  STACK_ALLOCATED();

 public:
  LineLayoutState(bool full_layout)
      : last_float_(nullptr),
        end_line_(nullptr),
        float_index_(0),
        end_line_matched_(false),
        has_inline_child_(false),
        is_full_layout_(full_layout),
        needs_pagination_strut_recalculation_(false) {}

  void MarkForFullLayout() { is_full_layout_ = true; }
  bool IsFullLayout() const { return is_full_layout_; }

  bool NeedsPaginationStrutRecalculation() const {
    return needs_pagination_strut_recalculation_ || IsFullLayout();
  }
  void SetNeedsPaginationStrutRecalculation() {
    needs_pagination_strut_recalculation_ = true;
  }

  bool EndLineMatched() const { return end_line_matched_; }
  void SetEndLineMatched(bool end_line_matched) {
    end_line_matched_ = end_line_matched;
  }

  bool HasInlineChild() const { return has_inline_child_; }
  void SetHasInlineChild(bool has_inline_child) {
    has_inline_child_ = has_inline_child;
  }

  LineInfo& GetLineInfo() { return line_info_; }
  const LineInfo& GetLineInfo() const { return line_info_; }

  LayoutUnit EndLineLogicalTop() const { return end_line_logical_top_; }
  void SetEndLineLogicalTop(LayoutUnit logical_top) {
    end_line_logical_top_ = logical_top;
  }

  RootInlineBox* EndLine() const { return end_line_; }
  void SetEndLine(RootInlineBox* line) { end_line_ = line; }

  FloatingObject* LastFloat() const { return last_float_; }
  void SetLastFloat(FloatingObject* last_float) { last_float_ = last_float; }

  Vector<LayoutBlockFlow::FloatWithRect>& Floats() { return floats_; }

  unsigned FloatIndex() const { return float_index_; }
  void SetFloatIndex(unsigned float_index) { float_index_ = float_index; }

  LayoutUnit AdjustedLogicalLineTop() const {
    return adjusted_logical_line_top_;
  }
  void SetAdjustedLogicalLineTop(LayoutUnit value) {
    adjusted_logical_line_top_ = value;
  }

 private:
  Vector<LayoutBlockFlow::FloatWithRect> floats_;
  FloatingObject* last_float_;
  RootInlineBox* end_line_;
  LineInfo line_info_;
  unsigned float_index_;
  LayoutUnit end_line_logical_top_;
  bool end_line_matched_;
  // Used as a performance optimization to avoid doing a full paint invalidation
  // when our floats change but we don't have any inline children.
  bool has_inline_child_;

  bool is_full_layout_;

  bool needs_pagination_strut_recalculation_;

  LayoutUnit adjusted_logical_line_top_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_LAYOUT_STATE_H_
