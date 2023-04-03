/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/line/line_width.h"

#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"

namespace blink {

LineWidth::LineWidth(LineLayoutBlockFlow block,
                     bool is_first_line,
                     IndentTextOrNot indent_text)
    : block_(block),
      uncommitted_width_(0),
      committed_width_(0),
      overhang_width_(0),
      trailing_whitespace_width_(0),
      is_first_line_(is_first_line),
      indent_text_(indent_text) {
  UpdateAvailableWidth();
}

void LineWidth::UpdateAvailableWidth(LayoutUnit replaced_height) {
  LayoutUnit height = block_.LogicalHeight();
  LayoutUnit logical_height =
      block_.MinLineHeightForReplacedObject(is_first_line_, replaced_height);
  left_ = block_.LogicalLeftOffsetForLine(height, IndentText(), logical_height);
  right_ =
      block_.LogicalRightOffsetForLine(height, IndentText(), logical_height);

  ComputeAvailableWidthFromLeftAndRight();
}

void LineWidth::Commit() {
  committed_width_ += uncommitted_width_;
  uncommitted_width_ = 0;
}

inline static LayoutUnit AvailableWidthAtOffset(
    LineLayoutBlockFlow block,
    const LayoutUnit& offset,
    IndentTextOrNot indent_text,
    LayoutUnit& new_line_left,
    LayoutUnit& new_line_right,
    const LayoutUnit& line_height = LayoutUnit()) {
  new_line_left =
      block.LogicalLeftOffsetForLine(offset, indent_text, line_height);
  new_line_right =
      block.LogicalRightOffsetForLine(offset, indent_text, line_height);
  return (new_line_right - new_line_left).ClampNegativeToZero();
}

void LineWidth::UpdateLineDimension(LayoutUnit new_line_top,
                                    LayoutUnit new_line_width,
                                    const LayoutUnit& new_line_left,
                                    const LayoutUnit& new_line_right) {
  if (new_line_width <= available_width_)
    return;

  block_.SetLogicalHeight(new_line_top);
  available_width_ =
      new_line_width + LayoutUnit::FromFloatCeil(overhang_width_);
  left_ = new_line_left;
  right_ = new_line_right;
}

void LineWidth::WrapNextToShapeOutside(bool is_first_line) {
  LayoutUnit line_height = block_.LineHeight(
      is_first_line,
      block_.IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine,
      kPositionOfInteriorLineBoxes);
  LayoutUnit line_logical_top = block_.LogicalHeight();
  LayoutUnit new_line_top = line_logical_top;
  LayoutUnit float_logical_bottom = line_logical_top;

  LayoutUnit new_line_width;
  LayoutUnit new_line_left = left_;
  LayoutUnit new_line_right = right_;
  while (true) {
    new_line_width =
        AvailableWidthAtOffset(block_, new_line_top, IndentText(),
                               new_line_left, new_line_right, line_height);
    if (new_line_width >= uncommitted_width_)
      break;

    if (new_line_top >= float_logical_bottom)
      break;

    new_line_top++;
  }
  UpdateLineDimension(new_line_top, LayoutUnit(new_line_width), new_line_left,
                      new_line_right);
}

void LineWidth::FitBelowFloats(bool is_first_line) {
  DCHECK(!committed_width_);
  DCHECK(!FitsOnLine());

  LayoutUnit last_float_logical_bottom = block_.LogicalHeight();
  LayoutUnit new_line_width = available_width_;
  LayoutUnit new_line_left = left_;
  LayoutUnit new_line_right = right_;

  UpdateLineDimension(last_float_logical_bottom, LayoutUnit(new_line_width),
                      new_line_left, new_line_right);
}

void LineWidth::ComputeAvailableWidthFromLeftAndRight() {
  available_width_ = (right_ - left_).ClampNegativeToZero() +
                     LayoutUnit::FromFloatCeil(overhang_width_);
}

}  // namespace blink
