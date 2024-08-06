// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/line_break_candidate.h"

#include "third_party/blink/renderer/core/layout/inline/inline_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_item_result.h"
#include "third_party/blink/renderer/core/layout/inline/line_breaker.h"
#include "third_party/blink/renderer/core/layout/inline/line_info.h"

namespace blink {

void LineBreakCandidateContext::Append(State new_state,
                                       InlineItemTextIndex offset,
                                       InlineItemTextIndex end,
                                       float pos_no_break,
                                       float pos_if_break,
                                       float penalty,
                                       bool is_hyphenated) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK_GE(offset, end);
  if (!candidates_.empty()) {
    const LineBreakCandidate& last_candidate = candidates_.back();
    if (state_ == LineBreakCandidateContext::kBreak) {
      DCHECK_GT(offset, last_candidate.offset);
    } else {
      DCHECK_GE(offset, last_candidate.offset);
    }
    DCHECK_GE(end, last_candidate.end);
    if (std::isnan(position_no_snap_)) {
      DCHECK(std::isnan(last_candidate.pos_no_break));
    } else if (position_no_snap_ < LayoutUnit::NearlyMax()) {
      DCHECK_EQ(position_no_snap_, last_candidate.pos_no_break);
      DCHECK_GE(pos_no_break, last_candidate.pos_no_break);
    }
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  switch (state_) {
    case State::kBreak:
      candidates_.emplace_back(offset, end, pos_no_break, pos_if_break, penalty,
                               is_hyphenated);
      break;
    case State::kMidWord: {
      LineBreakCandidate& last_candidate = candidates_.back();
      last_candidate.offset = offset;
      last_candidate.end = end;
      last_candidate.pos_no_break = pos_no_break;
      last_candidate.pos_if_break = pos_if_break;
      last_candidate.penalty = penalty;
      last_candidate.is_hyphenated = is_hyphenated;
      break;
    }
  }
  position_no_snap_ = pos_no_break;
  state_ = new_state;
}

void LineBreakCandidateContext::Append(State new_state,
                                       const InlineItemTextIndex& offset,
                                       float position) {
  Append(new_state, offset, offset, position, position);
}

void LineBreakCandidateContext::AppendTrailingSpaces(
    State new_state,
    const InlineItemTextIndex& offset,
    float pos_no_break) {
  DCHECK(!candidates_.empty());
  LineBreakCandidate& last_candidate = candidates_.back();
  DCHECK_GE(offset, last_candidate.offset);
  DCHECK_EQ(position_no_snap_, last_candidate.pos_no_break);
  last_candidate.offset = offset;
  last_candidate.pos_no_break = pos_no_break;
  position_no_snap_ = pos_no_break;
  state_ = new_state;
}

bool LineBreakCandidateContext::AppendLine(const LineInfo& line_info,
                                           LineBreaker& line_breaker) {
  const InlineItemResult& last_item_result = line_info.Results().back();
  if (!last_item_result.can_break_after) {
    // TODO(kojii): `last_item_result.can_break_after` should be true, but there
    // are cases where it is not set. The line breaker never uses it because
    // `can_break_after` is used for rewinding, but it helps simplifying this
    // logic.
    const_cast<InlineItemResult&>(last_item_result).can_break_after = true;
  }

  for (const InlineItemResult& item_result : line_info.Results()) {
    if (item_result.inline_size < LayoutUnit()) [[unlikely]] {
      // Negative margins are not supported, break opportunities must increase
      // monotonically. See `ScoreLineBreaker::ComputeScores`.
      return false;
    }
    DCHECK(item_result.item);
    const InlineItem& item = *item_result.item;
    switch (item.Type()) {
      case InlineItem::kText:
        line_breaker.AppendCandidates(item_result, line_info, *this);
        break;
      case InlineItem::kControl:
        AppendTrailingSpaces(item_result.can_break_after ? kBreak : kMidWord,
                             {item_result.item_index, item_result.EndOffset()},
                             SnappedPosition() + item_result.inline_size);
        SetLast(&item, item_result.EndOffset());
        break;
      default: {
        State new_state;
        if (item_result.can_break_after) {
          new_state = kBreak;
        } else if (state_ == kBreak) {
          new_state = kMidWord;
        } else {
          new_state = state_;
        }
        const InlineItemTextIndex offset{item_result.item_index + 1,
                                         item_result.EndOffset()};
        const float end_position = SnappedPosition() + item_result.inline_size;
        if (!item.Length()) {
          // Oopaque items such as open/close don't change `pos_if_break`,
          // similar to trailing spaces.
          const LineBreakCandidate& last_candidate = candidates_.back();
          Append(new_state, offset, last_candidate.end, end_position,
                 last_candidate.pos_if_break);
        } else {
          Append(new_state, offset, end_position);
        }
        SetLast(&item, item_result.EndOffset());
        break;
      }
    }
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  CheckConsistency();
  DCHECK_EQ(state_, kBreak);
  const LineBreakCandidate& last_candidate = candidates_.back();
  DCHECK_GE(last_candidate.offset.item_index, last_item_result.item_index);
  DCHECK_LE(last_candidate.offset.item_index, last_item_result.item_index + 1);
  DCHECK_GE(last_candidate.offset.text_offset, last_item_result.EndOffset());
  DCHECK_LE(last_candidate.offset.text_offset, line_info.EndTextOffset());
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
  return true;
}

void LineBreakCandidateContext::EnsureFirstSentinel(
    const LineInfo& first_line_info) {
  DCHECK(candidates_.empty());
  const InlineItemResult& first_item_result = first_line_info.Results().front();
  candidates_.push_back(LineBreakCandidate{first_item_result.Start(), 0});
#if EXPENSIVE_DCHECKS_ARE_ON()
  first_offset_ = first_item_result.Start();
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

void LineBreakCandidateContext::EnsureLastSentinel(
    const LineInfo& last_line_info) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  const InlineItemResult& last_item_result = last_line_info.Results().back();
  DCHECK(last_item_result.can_break_after);
  DCHECK_EQ(state_, LineBreakCandidateContext::kBreak);
  CheckConsistency();
  DCHECK_GE(candidates_.size(), 2u);
  DCHECK_EQ(candidates_.front().offset, first_offset_);
  DCHECK(candidates_.back().offset == last_item_result.End() ||
         candidates_.back().offset == last_line_info.End());
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
}

#if EXPENSIVE_DCHECKS_ARE_ON()
void LineBreakCandidateContext::CheckConsistency() const {
  for (wtf_size_t i = 1; i < candidates_.size(); ++i) {
    const LineBreakCandidate& candidate = candidates_[i];
    DCHECK_GE(candidate.offset, candidate.end);
    const LineBreakCandidate& prev_candidate = candidates_[i - 1];
    DCHECK_GT(candidate.offset, prev_candidate.offset);
    DCHECK_GE(candidate.end, prev_candidate.end);
    if (std::isnan(candidate.pos_no_break) ||
        candidate.pos_no_break >= LayoutUnit::NearlyMax()) {
      continue;
    }
    DCHECK_GE(candidate.pos_no_break, prev_candidate.pos_no_break);
  }
}
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

std::ostream& operator<<(std::ostream& ostream,
                         const LineBreakCandidate& candidate) {
  return ostream << candidate.offset << "/" << candidate.end << " "
                 << candidate.pos_no_break << "/" << candidate.pos_if_break
                 << " penalty=" << candidate.penalty
                 << (candidate.is_hyphenated ? " (hyphenated)" : "");
}

}  // namespace blink
