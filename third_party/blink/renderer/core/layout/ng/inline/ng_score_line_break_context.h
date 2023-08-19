// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_break_point.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_info_list.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// The maximum number of lines for the balancing and the optimal line breaking.
constexpr wtf_size_t kMaxLinesForBalance = 6;
constexpr wtf_size_t kMaxLinesForOptimal = 4;

using NGLineBreakPoints = Vector<NGLineBreakPoint, kMaxLinesForBalance>;

//
// Represents states and fields for `NGScoreLineBreaker` that should be kept
// across lines in an inline formatting context.
//
// Use `NGScoreLineBreakContextOf` to instantiate.
//
class CORE_EXPORT NGScoreLineBreakContext {
  STACK_ALLOCATED();

 public:
  NGLineInfoList& LineInfoList() { return line_info_list_; }

  NGLineBreakPoints& LineBreakPoints() { return line_break_points_; }
  wtf_size_t LineBreakPointsIndex() { return line_break_points_index_; }
  // Returns the current `NGLineBreakPoint` if it exists. The current is
  // incremented by `DidCreateLine()`.
  const NGLineBreakPoint* CurrentLineBreakPoint() const;

  // True if `NGScoreLineBreaker` can handle next line.
  bool IsActive() const { return line_break_points_.empty() && !is_suspended_; }
  // Suspend (make `IsActive()` false) until the end of the paragraph; i.e.,
  // either the end of the block or a forced break.
  void SuspendUntilEndParagraph() { is_suspended_ = true; }

  // Update states after a line was created.
  void DidCreateLine(bool is_end_paragraph);

 protected:
  explicit NGScoreLineBreakContext(NGLineInfoList& line_info_list)
      : line_info_list_(line_info_list) {}

 private:
  NGLineInfoList& line_info_list_;
  NGLineBreakPoints line_break_points_;
  wtf_size_t line_break_points_index_ = 0;
  bool is_suspended_ = false;
};

//
// Instantiate `NGScoreLineBreakContext` with the given `max_lines`.
//
template <wtf_size_t max_lines>
class CORE_EXPORT NGScoreLineBreakContextOf : public NGScoreLineBreakContext {
 public:
  NGScoreLineBreakContextOf()
      : NGScoreLineBreakContext(line_info_list_instance_) {}

 private:
  NGLineInfoListOf<max_lines> line_info_list_instance_;
};

inline const NGLineBreakPoint* NGScoreLineBreakContext::CurrentLineBreakPoint()
    const {
  if (line_break_points_.empty()) {
    return nullptr;
  }
  DCHECK_LT(line_break_points_index_, line_break_points_.size());
  return &line_break_points_[line_break_points_index_];
}

inline void NGScoreLineBreakContext::DidCreateLine(bool is_end_paragraph) {
  // Resume from the suspended state if all lines are consumed.
  if (UNLIKELY(is_suspended_ && is_end_paragraph)) {
    is_suspended_ = false;
  }

  // Advance the `CurrentLineBreakPoint()` to the next line.
  if (!line_break_points_.empty()) {
    DCHECK_LT(line_break_points_index_, line_break_points_.size());
    if (++line_break_points_index_ >= line_break_points_.size()) {
      line_break_points_.Shrink(0);
      line_break_points_index_ = 0;
    }
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAK_CONTEXT_H_
