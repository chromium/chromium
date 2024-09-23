// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/paragraph_line_breaker.h"

#include <numeric>
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"
#include "third_party/blink/renderer/core/layout/inline/score_line_break_context.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

struct LineBreakResult {
  LayoutUnit width;
};

struct LineBreakResults {
  STACK_ALLOCATED();

 public:
  LineBreakResults(const InlineNode& node, const ConstraintSpace& space)
      : node_(node), space_(space) {}

  wtf_size_t Size() const { return lines_.size(); }
  LayoutUnit LineWidthSum() const {
    return std::accumulate(lines_.begin(), lines_.end(), LayoutUnit(),
                           [](LayoutUnit acc, const LineBreakResult& item) {
                             return acc + item.width;
                           });
  }
  const InlineBreakToken* BreakToken() const { return break_token_; }

  void Clear() {
    break_token_ = nullptr;
    lines_.clear();
  }

  enum class Status {
    kFinished,          // Finished to the end or `stop_at`.
    kNotApplicable,     // This block is not applicable.
    kMaxLinesExceeded,  // # of lines exceeded `max_lines`.
  };

  Status BreakLines(const LayoutUnit available_width,
                    wtf_size_t max_lines,
                    const InlineBreakToken* stop_at = nullptr) {
    DCHECK(lines_.empty());
    const LineLayoutOpportunity line_opportunity(available_width);
    LeadingFloats leading_floats;
    ExclusionSpace exclusion_space;
    LineInfo line_info;
    for (;;) {
      LineBreaker line_breaker(node_, LineBreakerMode::kContent, space_,
                               line_opportunity, leading_floats, break_token_,
                               /* column_spanner_path_ */ nullptr,
                               &exclusion_space);
      line_breaker.NextLine(&line_info);
      // Bisecting can't find the desired value if the paragraph has forced line
      // breaks.
      DCHECK(!line_info.HasForcedBreak());
      if (line_breaker.ShouldDisableBisectLineBreak()) {
        return Status::kNotApplicable;
      }
      break_token_ = line_info.GetBreakToken();
      lines_.push_back(LineBreakResult{line_info.Width()});
      DCHECK_LE(lines_.size(), kMaxLinesForBalance);
      if (!break_token_ ||
          (stop_at && break_token_->Start() >= stop_at->Start())) {
        return Status::kFinished;
      }
      if (!--max_lines) {
        return Status::kMaxLinesExceeded;
      }
    }
  }

  LayoutUnit BisectAvailableWidth(const LayoutUnit max_available_width,
                                  const LayoutUnit min_available_width,
                                  const LayoutUnit epsilon,
                                  const wtf_size_t num_lines,
                                  const InlineBreakToken* stop_at = nullptr) {
    DCHECK_GT(epsilon, LayoutUnit());  // 0 may cause an infinite loop
    DCHECK_GT(num_lines, 0u);
    DCHECK_EQ(Size(), 0u);
    LayoutUnit upper = max_available_width;
    LayoutUnit lower = min_available_width;
    while (lower + epsilon < upper) {
      const LayoutUnit middle = (upper + lower) / 2;
      const Status status = BreakLines(middle, num_lines, stop_at);
      if (status != Status::kFinished) {
        lower = middle;
      } else {
        DCHECK_LE(Size(), num_lines);
        upper = middle;
      }
      Clear();
    }
    DCHECK_GE(upper, min_available_width);
    DCHECK_LE(upper, max_available_width);
    return upper;
  }

 private:
  const InlineNode node_;
  const ConstraintSpace& space_;
  Vector<LineBreakResult, kMaxLinesForBalance> lines_;
  const InlineBreakToken* break_token_ = nullptr;
};

// Estimate the number of lines using the `ch` unit (the space width) without
// running the line breaker.
wtf_size_t EstimateNumLines(const String& text_content,
                            const SimpleFontData* font,
                            LayoutUnit available_width) {
  const float space_width = font->SpaceWidth();
  if (space_width <= 0) {
    // Can't estimate without space glyph, go on to measure the actual value.
    return 0;
  }
  const wtf_size_t num_line_chars = available_width / space_width;
  if (num_line_chars <= 0) {
    // The width is too narrow, don't balance.
    return std::numeric_limits<wtf_size_t>::max();
  }
  return (text_content.length() + num_line_chars - 1) / num_line_chars;
}

}  // namespace

// static
std::optional<LayoutUnit> ParagraphLineBreaker::AttemptParagraphBalancing(
    const InlineNode& node,
    const ConstraintSpace& space,
    const LineLayoutOpportunity& line_opportunity) {
  if (node.IsBisectLineBreakDisabled()) {
    return std::nullopt;
  }

  const ComputedStyle& block_style = node.Style();
  const LayoutUnit available_width = line_opportunity.AvailableInlineSize();
  LineBreakResults normal_lines(node, space);
  constexpr wtf_size_t max_lines = kMaxLinesForBalance;
  const int lines_until_clamp =
      space.GetLineClampData().LinesUntilClamp().value_or(0);
  if (lines_until_clamp > 0 &&
      static_cast<unsigned>(lines_until_clamp) <= max_lines) {
    if (lines_until_clamp == 1) {
      return std::nullopt;  // Balancing not needed for single line paragraphs.
    }

    const LineBreakResults::Status status =
        normal_lines.BreakLines(available_width, lines_until_clamp);
    if (status == LineBreakResults::Status::kNotApplicable) {
      return std::nullopt;
    }
  } else {
    // Estimate the number of lines to see if the text is too long to balance.
    // Because this is an estimate, allow it to be `max_lines * 2`.
    const InlineItemsData& items_data = node.ItemsData(
        /* use_first_line_style */ false);
    const wtf_size_t estimated_num_lines = EstimateNumLines(
        items_data.text_content, block_style.GetFont().PrimaryFont(),
        line_opportunity.AvailableInlineSize());
    if (estimated_num_lines > max_lines * 2) {
      return std::nullopt;
    }

    const LineBreakResults::Status status =
        normal_lines.BreakLines(available_width, max_lines);
    if (status != LineBreakResults::Status::kFinished) {
      // Abort if not applicable or `max_lines` exceeded.
      return std::nullopt;
    }
    DCHECK(!normal_lines.BreakToken());
  }
  const wtf_size_t num_lines = normal_lines.Size();
  DCHECK_LE(num_lines, max_lines);
  if (num_lines <= 1) {
    return std::nullopt;  // Balancing not needed for single line paragraphs.
  }

  // The bisect less than 1 pixel is worthless, so ignore. Use CSS pixels
  // instead of device pixels to make the algorithm consistent across different
  // zoom levels, but make sure it's not zero to avoid infinite loop.
  const LayoutUnit epsilon =
      LayoutUnit::FromFloatCeil(block_style.EffectiveZoom());

  // Find the desired available width by bisecting the maximum available width
  // that produces `num_lines`.
  LineBreakResults balanced_lines(node, space);
  // Start the bisect with the minimum value at the average line width, with 20%
  // buffer for potential edge cases.
  const LayoutUnit avg_line_width = normal_lines.LineWidthSum() / num_lines;
  const LayoutUnit min_available_width =
      LayoutUnit::FromFloatRound(avg_line_width * .8f);
  return balanced_lines.BisectAvailableWidth(
      available_width, min_available_width, epsilon, num_lines,
      normal_lines.BreakToken());
}

}  // namespace blink
