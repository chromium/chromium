// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_RUBY_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_RUBY_UTILS_H_

#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class ComputedStyle;
class LineInfo;
class LogicalLineItems;
class ShapeResultView;
struct FontHeight;
struct InlineItemResult;
struct PhysicalRect;

// Adjust the specified |rect| of a text fragment for 'em' height.
// This is called on computing scrollable overflow with kEmHeight.
PhysicalRect AdjustTextRectForEmHeight(const PhysicalRect& rect,
                                       const ComputedStyle& style,
                                       const ShapeResultView* shape_view,
                                       WritingMode writing_mode);

struct AnnotationOverhang {
  LayoutUnit start;
  LayoutUnit end;
};

// Returns overhang values of the specified InlineItemResult representing
// LayoutRubyColumn.
//
// This is used by LineBreaker.
AnnotationOverhang GetOverhang(const InlineItemResult& item);

// Returns true if |start_overhang| is applied to a previous item, and
// clamp |start_overhang| to the width of the previous item.
//
// This is used by LineBreaker.
bool CanApplyStartOverhang(const LineInfo& line_info,
                           LayoutUnit& start_overhang);

// This should be called after InlineItemResult for a text is added in
// LineBreaker::HandleText().
//
// This function may update a InlineItemResult representing RubyColumn
// in |line_info|
LayoutUnit CommitPendingEndOverhang(LineInfo* line_info);

// Stores ComputeAnnotationOverflow() results.
//
// |overflow_over| and |space_over| are exclusive. Only one of them can be
// non-zero. |overflow_under| and |space_under| are exclusive too.
// All fields never be negative.
struct AnnotationMetrics {
  // The amount of annotation overflow at the line-over side.
  LayoutUnit overflow_over;
  // The amount of annotation overflow at the line-under side.
  LayoutUnit overflow_under;
  // The amount of annotation space which the next line at the line-over
  // side can consume.
  LayoutUnit space_over;
  // The amount of annotation space which the next line at the line-under
  // side can consume.
  LayoutUnit space_under;
};

// Compute over/under annotation overflow/space for the specified line.
AnnotationMetrics ComputeAnnotationOverflow(
    const LogicalLineItems& logical_line,
    const FontHeight& line_box_metrics,
    const ComputedStyle& line_style);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_RUBY_UTILS_H_
