// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAKER_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_break_candidate.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_score_line_break_context.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class NGConstraintSpace;
class NGInlineBreakToken;
class NGInlineNode;
class NGLineInfoList;
struct NGLineLayoutOpportunity;

//
// This class computes line break points using penalties and scores, similar to
// the Knuth's TeX algorithm.
//
// In short, the algorithm works in following steps:
// 1. It runs `NGLineBreaker` to compute line break points greedy.
// 2. If the result doesn't meet the criteria to apply this score-based line
//    breaking, it returns the result without applying the algorithm.
// 3. It then computes all break candidates (a.k.a., break opportunities) with
//    penalties from the greedy results.
// 4. It then computes the scores for all break candidates.
// 5. The break candidates of the highest score is determined as the line break
//    points.
//
// This algorithm is based on Android's `LineBreak.Strategy.HighQuality`:
// https://cs.android.com/android/platform/superproject/+/master:frameworks/minikin/libs/minikin/OptimalLineBreaker.cpp
//
class CORE_EXPORT NGScoreLineBreaker {
  STACK_ALLOCATED();

 public:
  NGScoreLineBreaker(const NGInlineNode& node,
                     const NGConstraintSpace& space,
                     const NGLineLayoutOpportunity& line_opportunity)
      : node_(node), space_(space), line_opportunity_(line_opportunity) {
    DCHECK(!node.IsScoreLineBreakDisabled());
  }

  // The primary entry point of doing all the work described in the class
  // comment.
  void OptimalBreakPoints(const NGInlineBreakToken* break_token,
                          NGScoreLineBreakContext& context);

  // Makes the length of all lines balanced, by running the `OptimalBreakPoints`
  // with a higher penalty for the end of the paragraph.
  void BalanceBreakPoints(NGScoreLineBreakContext& context);

 private:
  struct NGLineBreakScore {
    float score = 0;            // best score found for this break
    wtf_size_t prev_index = 0;  // index to previous break
    wtf_size_t line_index = 0;  // the computed line number of the candidate
  };
  using NGLineBreakScores =
      Vector<NGLineBreakScore, NGLineBreakCandidate::kInlineCapacity>;

  const NGInlineNode& Node() const { return node_; }
  const NGConstraintSpace& ConstraintSpace() const { return space_; }
  LayoutUnit AvailableWidth(wtf_size_t line_index) const;
  LayoutUnit AvailableWidthToFit(wtf_size_t line_index) const {
    return AvailableWidth(line_index).AddEpsilon();  // Match `NGLineBreaker`.
  }

  bool Optimize(const NGLineInfoList& line_info_list,
                NGLineBreaker& line_breaker,
                NGLineBreakPoints& break_points);
  bool ComputeCandidates(const NGLineInfoList& line_info_list,
                         NGLineBreaker& line_breaker,
                         NGLineBreakCandidates& candidates);
  void ComputeLineWidths(const NGLineInfoList& line_info_list);
  void ComputeScores(const NGLineBreakCandidates& candidates,
                     NGLineBreakScores& scores);
  void ComputeBreakPoints(const NGLineBreakCandidates& candidates,
                          const NGLineBreakScores& scores,
                          NGLineBreakPoints& break_points);

  static constexpr float kScoreInfinity = std::numeric_limits<float>::max();
  static constexpr float kScoreOverfull = 1e12f;
  static constexpr float kLastLinePenaltyMultiplier = 4.0f;

  const NGInlineNode node_;
  const NGConstraintSpace& space_;
  const NGLineLayoutOpportunity& line_opportunity_;
  LayoutUnit first_line_indent_;
  bool is_balanced_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_SCORE_LINE_BREAKER_H_
