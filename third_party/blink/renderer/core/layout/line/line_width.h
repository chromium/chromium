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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_WIDTH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_WIDTH_H_

#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FloatingObject;
class LineLayoutRubyRun;

enum WhitespaceTreatment { kExcludeWhitespace, kIncludeWhitespace };

class LineWidth {
  STACK_ALLOCATED();

 public:
  LineWidth(LineLayoutBlockFlow, bool is_first_line, IndentTextOrNot);

  bool FitsOnLine() const {
    return LayoutUnit::FromFloatFloor(CurrentWidth()) <=
           available_width_ + LayoutUnit::Epsilon();
  }
  bool FitsOnLine(float extra) const {
    float total_width = CurrentWidth() + extra;
    return LayoutUnit::FromFloatFloor(total_width) <=
           available_width_ + LayoutUnit::Epsilon();
  }
  bool FitsOnLine(float extra, WhitespaceTreatment whitespace_treatment) const {
    LayoutUnit w = LayoutUnit::FromFloatFloor(CurrentWidth() + extra);
    if (whitespace_treatment == kExcludeWhitespace)
      w -= LayoutUnit::FromFloatCeil(TrailingWhitespaceWidth());
    return w <= available_width_;
  }

  // Note that m_uncommittedWidth may not be LayoutUnit-snapped at this point.
  // Because currentWidth() is used by the code that lays out words in a single
  // LayoutText, it's expected that offsets will not be snapped until an
  // InlineBox boundary is reached.
  float CurrentWidth() const { return committed_width_ + uncommitted_width_; }

  // FIXME: We should eventually replace these three functions by ones that work
  // on a higher abstraction.
  float UncommittedWidth() const { return uncommitted_width_; }
  float CommittedWidth() const { return committed_width_; }
  float AvailableWidth() const { return available_width_; }
  float TrailingWhitespaceWidth() const { return trailing_whitespace_width_; }

  void UpdateAvailableWidth(LayoutUnit minimum_height = LayoutUnit());
  void ShrinkAvailableWidthForNewFloatIfNeeded(const FloatingObject&);
  void AddUncommittedWidth(float delta) { uncommitted_width_ += delta; }
  void Commit();
  void ApplyOverhang(LineLayoutRubyRun,
                     LineLayoutItem start_layout_item,
                     LineLayoutItem end_layout_item);
  void FitBelowFloats(bool is_first_line = false);
  void SetTrailingWhitespaceWidth(float width) {
    trailing_whitespace_width_ = width;
  }
  void SnapAtNodeBoundary() {
    if (!uncommitted_width_) {
      committed_width_ = LayoutUnit::FromFloatCeil(committed_width_).ToFloat();
    } else {
      uncommitted_width_ =
          LayoutUnit::FromFloatCeil(committed_width_ + uncommitted_width_)
              .ToFloat() -
          committed_width_;
    }
  }

  IndentTextOrNot IndentText() const { return indent_text_; }

 private:
  void ComputeAvailableWidthFromLeftAndRight();
  void UpdateLineDimension(LayoutUnit new_line_top,
                           LayoutUnit new_line_width,
                           const LayoutUnit& new_line_left,
                           const LayoutUnit& new_line_right);
  void WrapNextToShapeOutside(bool is_first_line);

  LineLayoutBlockFlow block_;
  float uncommitted_width_;
  float committed_width_;
  // The amount by which |m_availableWidth| has been inflated to account for
  // possible contraction due to ruby overhang.
  float overhang_width_;
  float trailing_whitespace_width_;
  LayoutUnit left_;
  LayoutUnit right_;
  LayoutUnit available_width_;
  bool is_first_line_;
  IndentTextOrNot indent_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_LINE_WIDTH_H_
