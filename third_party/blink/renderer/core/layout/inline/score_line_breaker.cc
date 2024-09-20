// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/score_line_breaker.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_break_token.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info_list.h"
#include "third_party/blink/renderer/core/layout/inline/line_widths.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

namespace blink {

namespace {

// Determines whether `ScoreLineBreaker` should be applied or not from the
// greedy line break results. Because the `ScoreLineBreaker` is expensive,
// and it often produces the similar results to the greedy algorithm, apply
// it only when its benefit is obvious.
ALWAYS_INLINE bool ShouldOptimize(const LineInfoList& line_info_list,
                                  LineBreaker& line_breaker) {
  // The optimization benefit is most visible when the last line is short.
  // Otherwise, the improvement is not worth the performance impact.
  const LineInfo& last_line = line_info_list.Back();
  constexpr int kShortLineDenominator = 3;
  if (last_line.Width() < last_line.AvailableWidth() / kShortLineDenominator &&
      // Similarly, optimize only when the last line has a single word; i.e.,
      // has no break opportunities. This takes some cost, but the performance
      // improvement by reducing the applicability wins over the cost.
      !line_breaker.CanBreakInside(last_line)) {
    return true;
  }

  // Hyphenating the second to last line is not desirable, and consecutive
  // hyphenated lines are not desirable either. For now, apply only if both
  // occur, to minimize the performance impact.
  constexpr wtf_size_t kNumLastHyphenatedLines = 2;
  const wtf_size_t num_lines = line_info_list.Size();
  if (num_lines >= kNumLastHyphenatedLines + 1 &&
      line_info_list[num_lines - 2].IsHyphenated() &&
      line_info_list[num_lines - 3].IsHyphenated()) {
    return true;
  }

  return false;
}

}  // namespace

void ScoreLineBreaker::SetScoresOutForTesting(Vector<float>* scores_out) {
  scores_out_for_testing_ = scores_out;
}

void ScoreLineBreaker::OptimalBreakPoints(const LeadingFloats& leading_floats,
                                          ScoreLineBreakContext& context) {
  DCHECK(!is_balanced_ || !break_token_);
  DCHECK(context.GetLineBreakPoints().empty());
  DCHECK(!node_.IsScoreLineBreakDisabled());
  DCHECK(context.IsActive());
  LineInfoList& line_info_list = context.GetLineInfoList();
  const wtf_size_t max_lines = MaxLines();
  DCHECK_GE(line_info_list.MaxLines(), max_lines);
  DCHECK_LT(line_info_list.Size(), max_lines);
  wtf_size_t line_index = 0;
  if (!line_info_list.IsEmpty()) {
    line_index = line_info_list.Size();
    // To compute the next line after the last cached line, update
    // `break_token_` to the last cached break token.
    const LineInfo& last_line = line_info_list.Back();
    break_token_ = last_line.GetBreakToken();
    // The last line should not be the end of paragraph.
    // `SuspendUntilConsumed()` should have prevented this from happening.
    DCHECK(break_token_ && !last_line.HasForcedBreak());
  }

  // Compute line breaks and cache the results (`LineInfo`) up to
  // `LineInfoList::kCapacity` lines.
  LayoutUnit line_width = line_widths_[line_index];
  LineBreaker line_breaker(
      node_, LineBreakerMode::kContent, GetConstraintSpace(),
      LineLayoutOpportunity(line_width), leading_floats, break_token_,
      /* column_spanner_path */ nullptr, exclusion_space_);
  const int lines_until_clamp =
      space_.GetLineClampData().LinesUntilClamp().value_or(0);
  for (;;) {
    LineInfo& line_info = line_info_list.Append();
    line_breaker.NextLine(&line_info);
    break_token_ = line_info.GetBreakToken();
    if (line_breaker.ShouldDisableScoreLineBreak()) [[unlikely]] {
      context.SuspendUntilEndParagraph();
      return;
    }
    if (line_info.IsEndParagraph() || [&] {
          if (lines_until_clamp > 0 &&
              line_info_list.Size() ==
                  static_cast<wtf_size_t>(lines_until_clamp)) [[unlikely]] {
            return true;
          }
          return false;
        }()) {
      context.SuspendUntilEndParagraph();
      break;
    }
    DCHECK(!line_info.Results().empty());
    DCHECK(!line_breaker.IsFinished());
    if (line_info_list.Size() >= max_lines) {
      return;
    }

    const LayoutUnit next_line_width = line_widths_[++line_index];
    if (next_line_width != line_width) {
      line_width = next_line_width;
      line_breaker.SetLineOpportunity(LineLayoutOpportunity(line_width));
    }
  }
  DCHECK(!line_info_list.IsEmpty());

  // Now we have a "paragraph" in `line_info_list`; i.e., a block, or a part
  // of a block segmented by forced line breaks.
  if (line_info_list.Size() <= 1) {
    return;  // Optimization not needed for single line paragraphs.
  }
  if (!is_balanced_) {
    if (!ShouldOptimize(line_info_list, line_breaker)) {
      return;
    }
  }

  LineBreakPoints& break_points = context.GetLineBreakPoints();
  if (!Optimize(line_info_list, line_breaker, break_points)) {
    DCHECK(break_points.empty());
    return;
  }
  DCHECK(!break_points.empty());

  // If succeeded, clear the previously computed `line_info_list` if optimized
  // break points are different.
  DCHECK_EQ(line_info_list.Size(), break_points.size());
  for (wtf_size_t i = 0; i < line_info_list.Size(); ++i) {
    const LineInfo& line_info = line_info_list[i];
    if (line_info.End() != break_points[i].offset) {
      line_info_list.Shrink(i);
      break;
    }
  }
}

void ScoreLineBreaker::BalanceBreakPoints(const LeadingFloats& leading_floats,
                                          ScoreLineBreakContext& context) {
  is_balanced_ = true;
  OptimalBreakPoints(leading_floats, context);
}

bool ScoreLineBreaker::Optimize(const LineInfoList& line_info_list,
                                LineBreaker& line_breaker,
                                LineBreakPoints& break_points) {
  DCHECK(break_points.empty());

  SetupParameters();

  // Compute all break opportunities and their penalties.
  LineBreakCandidates candidates;
  if (!ComputeCandidates(line_info_list, line_breaker, candidates)) {
    DCHECK(break_points.empty());
    return false;
  }

  // Optimization not needed if one or no break opportunities in the paragraph.
  // The `candidates` has sentinels, one at the front and one at the back, so
  // `2` means no break opportunities, `3` means one.
  DCHECK_GE(candidates.size(), 2u);
  constexpr wtf_size_t kMinCandidates = 3;
  if (candidates.size() < kMinCandidates + 2) {
    DCHECK(break_points.empty());
    return false;
  }

  if (candidates.size() >= 4) {
    // Increase penalties to minimize typographic orphans.
    constexpr float kOrphansPenalty = 10000;
    const float orphans_penalty = kOrphansPenalty * zoom_;
    const auto candidates_span =
        base::span(candidates).first(candidates.size() - 1);
    for (LineBreakCandidate& candidate : base::Reversed(candidates_span)) {
      candidate.penalty += orphans_penalty;
      if (!candidate.is_hyphenated) {
        break;
      }
    }
  }

  ComputeLineWidths(line_info_list);

  // Compute score for each break opportunity.
  LineBreakScores scores;
  scores.ReserveInitialCapacity(candidates.size());
  ComputeScores(candidates, scores);
  DCHECK_EQ(candidates.size(), scores.size());

  // Determine final break points.
  ComputeBreakPoints(candidates, scores, break_points);

  // Copy data for testing.
  if (scores_out_for_testing_) [[unlikely]] {
    for (const LineBreakScore& score : scores) {
      scores_out_for_testing_->push_back(score.score);
    }
  }

  return true;
}

bool ScoreLineBreaker::ComputeCandidates(const LineInfoList& line_info_list,
                                         LineBreaker& line_breaker,
                                         LineBreakCandidates& candidates) {
  // The first entry is a sentinel at the start of the line.
  DCHECK(candidates.empty());
  LineBreakCandidateContext context(candidates);
  context.SetHyphenPenalty(hyphen_penalty_);
  context.EnsureFirstSentinel(line_info_list.Front());

  for (wtf_size_t i = 0; i < line_info_list.Size(); ++i) {
    const LineInfo& line_info = line_info_list[i];
    if (!context.AppendLine(line_info, line_breaker)) {
      candidates.clear();
      return false;
    }
  }

  // The last entry is a sentinel at the end of the line.
  context.EnsureLastSentinel(line_info_list.Back());
  return true;
}

LayoutUnit ScoreLineBreaker::AvailableWidth(wtf_size_t line_index) const {
  LayoutUnit available_width = line_widths_[line_index];
  if (line_index == 0) {
    available_width -= first_line_indent_;
  }
  return available_width.ClampNegativeToZero();
}

void ScoreLineBreaker::ComputeLineWidths(const LineInfoList& line_info_list) {
  first_line_indent_ = line_info_list.Front().TextIndent();
#if EXPENSIVE_DCHECKS_ARE_ON()
  // Only the first line may have an indent.
  for (wtf_size_t i = 1; i < line_info_list.Size(); ++i) {
    DCHECK_EQ(line_info_list[i].TextIndent(), LayoutUnit());
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

void ScoreLineBreaker::SetupParameters() {
  // Use the same heuristic parameters as Minikin's `computePenalties()`.
  // https://cs.android.com/android/platform/superproject/+/master:frameworks/minikin/libs/minikin/OptimalLineBreaker.cpp
  const LayoutUnit available_width =
      line_widths_.Default().ClampNegativeToZero();
  const ComputedStyle& block_style = node_.Style();
  const float font_size = block_style.GetFontDescription().ComputedSize();
  zoom_ = block_style.EffectiveZoom();
  DCHECK_GT(zoom_, .0f);
  // Penalties/scores should be a zoomed value. Because both `font_size` and
  // `available_width` are zoomed, unzoom once.
  const float width_times_font_size =
      available_width.ToFloat() * font_size / zoom_;
  is_justified_ = block_style.GetTextAlign() == ETextAlign::kJustify;
  if (is_justified_) {
    // For justified text, make hyphenation more aggressive and no line penalty.
    hyphen_penalty_ = width_times_font_size / 2;
    line_penalty_ = .0f;
  } else {
    hyphen_penalty_ = width_times_font_size * 2;
    line_penalty_ = hyphen_penalty_ * 2;
  }
}

void ScoreLineBreaker::ComputeScores(const LineBreakCandidates& candidates,
                                     LineBreakScores& scores) {
  DCHECK_GE(candidates.size(), 2u);
  DCHECK(scores.empty());
  scores.push_back(LineBreakScore{0, 0, 0});
  wtf_size_t active = 0;

  // `end` iterates through candidates for the end of the line.
  for (wtf_size_t end = 1; end < candidates.size(); ++end) {
    const LineBreakCandidate& end_candidate = candidates[end];
    const bool is_end_last_candidate = end == candidates.size() - 1;
    float best = kScoreInfinity;
    wtf_size_t best_prev_index = 0;

    wtf_size_t last_line_index = scores[active].line_index;
    LayoutUnit available_width = AvailableWidthToFit(last_line_index);
    float start_edge = end_candidate.pos_if_break - available_width;
    float best_hope = 0;

    // `start` iterates through candidates for the beginning of the line, to
    // determine the best score for the `end`.
    for (wtf_size_t start = active; start < end; ++start) {
      const LineBreakScore& start_score = scores[start];
      const wtf_size_t line_index = start_score.line_index;
      if (line_index != last_line_index) {
        last_line_index = line_index;
        const LayoutUnit new_available_width = AvailableWidthToFit(line_index);
        if (new_available_width != available_width) {
          available_width = new_available_width;
          start_edge = end_candidate.pos_if_break - available_width;
          best_hope = 0;
        }
      }
      const float start_score_value = start_score.score;
      if (start_score_value + best_hope >= best) {
        continue;
      }
      const LineBreakCandidate& start_candidate = candidates[start];
      const float delta = start_candidate.pos_no_break - start_edge;

      // Compute width score for line.

      // Note: the "best_hope" optimization makes the assumption that, when
      // delta is non-negative, width_score will increase monotonically as
      // successive candidate breaks are considered.
      float width_score = 0;
      float additional_penalty = 0;
      if ((is_end_last_candidate || !is_justified_) && delta < 0) {
        width_score = kScoreOverfull;
      } else if (is_end_last_candidate && !is_balanced_) {
        // Increase penalty for hyphen on last line.
        // TODO(kojii): Review the penalty value.
        additional_penalty =
            kLastLinePenaltyMultiplier * start_candidate.penalty;
      } else if (delta < 0) {
        width_score = kScoreOverfull;
      } else {
        // Penalties/scores should be a zoomed value. Because `delta` is zoomed,
        // unzoom once.
        width_score = delta * delta / zoom_;
      }
      if (delta < 0) {
        active = start + 1;
      } else {
        best_hope = width_score;
      }
      const float score = start_score_value + width_score + additional_penalty;
      if (score <= best) {
        best = score;
        best_prev_index = start;
      }
    }

    scores.push_back(LineBreakScore{
        best + end_candidate.penalty + line_penalty_, best_prev_index,
        scores[best_prev_index].line_index + 1});
  }
}

void ScoreLineBreaker::ComputeBreakPoints(const LineBreakCandidates& candidates,
                                          const LineBreakScores& scores,
                                          LineBreakPoints& break_points) {
  DCHECK_GE(candidates.size(), 3u);
  DCHECK_EQ(candidates.size(), scores.size());
  DCHECK(break_points.empty());
  DCHECK_LE(scores.back().line_index, MaxLines());

  for (wtf_size_t i = scores.size() - 1, prev_index; i > 0; i = prev_index) {
    prev_index = scores[i].prev_index;
    const LineBreakCandidate& candidate = candidates[i];
    break_points.push_back(candidate);
#if EXPENSIVE_DCHECKS_ARE_ON()
    const LineBreakCandidate& prev_candidate = candidates[prev_index];
    const LayoutUnit line_width = LayoutUnit::FromFloatCeil(
        candidate.pos_if_break - prev_candidate.pos_no_break);
    DCHECK_GE(line_width, 0);
    break_points.back().line_width = line_width;
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  }
  DCHECK_EQ(break_points.size(), scores.back().line_index);

  // `break_points` is in the descending order. Reverse it.
  break_points.Reverse();

#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK_EQ(break_points.size(), scores.back().line_index);
  for (wtf_size_t i = 1; i < break_points.size(); ++i) {
    DCHECK_GT(break_points[i].offset, break_points[i - 1].offset);
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

}  // namespace blink
