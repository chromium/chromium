// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/state_machines/forward_grapheme_boundary_state_machine.h"

#include <ostream>

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_util.h"
#include "third_party/blink/renderer/core/editing/state_machines/text_segmentation_machine_state.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {
namespace {
const UChar32 kUnsetCodePoint = WTF::unicode::kMaxCodepoint + 1;
}  // namespace

#define FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)                    \
  /* Counting preceding regional indicators. This is initial state. */ \
  V(kCountRIS)                                                         \
  /* Waiting lead surrogate during counting regional indicators. */    \
  V(kCountRISWaitLeadSurrogate)                                        \
  /* Waiting first following code unit. */                             \
  V(kStartForward)                                                     \
  /* Waiting trail surrogate for the first following code point. */    \
  V(kStartForwardWaitTrailSurrgate)                                    \
  /* Searching grapheme boundary. */                                   \
  V(kSearch)                                                           \
  /* Waiting trail surrogate during searching grapheme boundary. */    \
  V(kSearchWaitTrailSurrogate)                                         \
  /* The state machine has stopped. */                                 \
  V(kFinished)

enum class ForwardGraphemeBoundaryStateMachine::InternalState {
#define V(name) name,
  FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
};

std::ostream& operator<<(
    std::ostream& os,
    ForwardGraphemeBoundaryStateMachine::InternalState state) {
  static const char* const kTexts[] = {
#define V(name) #name,
      FOR_EACH_FORWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
  };
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown state value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown state value";
  return os << *it;
}

ForwardGraphemeBoundaryStateMachine::ForwardGraphemeBoundaryStateMachine()
    : prev_code_point_(kUnsetCodePoint),
      internal_state_(InternalState::kCountRIS) {}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::FeedPrecedingCodeUnit(UChar code_unit) {
  DCHECK_EQ(prev_code_point_, kUnsetCodePoint);
  DCHECK_EQ(boundary_offset_, 0);
  switch (internal_state_) {
    case InternalState::kCountRIS:
      DCHECK_EQ(pending_code_unit_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        pending_code_unit_ = code_unit;
        return MoveToNextState(InternalState::kCountRISWaitLeadSurrogate);
      }
      return MoveToNextState(InternalState::kStartForward);
    case InternalState::kCountRISWaitLeadSurrogate:
      DCHECK_NE(pending_code_unit_, 0);
      if (U16_IS_LEAD(code_unit)) {
        const UChar32 code_point =
            U16_GET_SUPPLEMENTARY(code_unit, pending_code_unit_);
        pending_code_unit_ = 0;
        if (Character::IsRegionalIndicator(code_point)) {
          ++preceding_ris_count_;
          return MoveToNextState(InternalState::kCountRIS);
        }
      }
      pending_code_unit_ = 0;
      return MoveToNextState(InternalState::kStartForward);
    case InternalState::kStartForward:                   // Fallthrough
    case InternalState::kStartForwardWaitTrailSurrgate:  // Fallthrough
    case InternalState::kSearch:                         // Fallthrough
    case InternalState::kSearchWaitTrailSurrogate:       // Fallthrough
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedPrecedingCodeUnit() once "
          << TextSegmentationMachineState::kNeedFollowingCodeUnit
          << " is returned. InternalState: " << internal_state_;
      return Finish();
    case InternalState::kFinished:
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedPrecedingCodeUnit() once it finishes.";
      return Finish();
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled state: " << internal_state_;
  return Finish();
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::FeedFollowingCodeUnit(UChar code_unit) {
  switch (internal_state_) {
    case InternalState::kCountRIS:  // Fallthrough
    case InternalState::kCountRISWaitLeadSurrogate:
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedFollowingCodeUnit() until "
          << TextSegmentationMachineState::kNeedFollowingCodeUnit
          << " is returned. InternalState: " << internal_state_;
      return Finish();
    case InternalState::kStartForward:
      DCHECK_EQ(prev_code_point_, kUnsetCodePoint);
      DCHECK_EQ(boundary_offset_, 0);
      DCHECK_EQ(pending_code_unit_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        // Lonely trail surrogate.
        boundary_offset_ = 1;
        return Finish();
      }
      if (U16_IS_LEAD(code_unit)) {
        pending_code_unit_ = code_unit;
        return MoveToNextState(InternalState::kStartForwardWaitTrailSurrgate);
      }
      prev_code_point_ = code_unit;
      boundary_offset_ = 1;
      return MoveToNextState(InternalState::kSearch);
    case InternalState::kStartForwardWaitTrailSurrgate:
      DCHECK_EQ(prev_code_point_, kUnsetCodePoint);
      DCHECK_EQ(boundary_offset_, 0);
      DCHECK_NE(pending_code_unit_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        prev_code_point_ = U16_GET_SUPPLEMENTARY(pending_code_unit_, code_unit);
        boundary_offset_ = 2;
        pending_code_unit_ = 0;
        return MoveToNextState(InternalState::kSearch);
      }
      // Lonely lead surrogate.
      boundary_offset_ = 1;
      return Finish();
    case InternalState::kSearch:
      DCHECK_NE(prev_code_point_, kUnsetCodePoint);
      DCHECK_NE(boundary_offset_, 0);
      DCHECK_EQ(pending_code_unit_, 0);
      if (U16_IS_LEAD(code_unit)) {
        pending_code_unit_ = code_unit;
        return MoveToNextState(InternalState::kSearchWaitTrailSurrogate);
      }
      if (U16_IS_TRAIL(code_unit))
        return Finish();  // Lonely trail surrogate.
      if (IsGraphemeBreak(prev_code_point_, code_unit))
        return Finish();
      prev_code_point_ = code_unit;
      boundary_offset_ += 1;
      return StaySameState();
    case InternalState::kSearchWaitTrailSurrogate:
      DCHECK_NE(prev_code_point_, kUnsetCodePoint);
      DCHECK_NE(boundary_offset_, 0);
      DCHECK_NE(pending_code_unit_, 0);
      if (!U16_IS_TRAIL(code_unit))
        return Finish();  // Lonely lead surrogate.

      {
        const UChar32 code_point =
            U16_GET_SUPPLEMENTARY(pending_code_unit_, code_unit);
        pending_code_unit_ = 0;
        if (Character::IsRegionalIndicator(prev_code_point_) &&
            Character::IsRegionalIndicator(code_point)) {
          if (preceding_ris_count_ % 2 == 0) {
            // Odd numbered RI case, note that prev_code_point_ is also RI.
            boundary_offset_ += 2;
          }
          return Finish();
        }
        if (IsGraphemeBreak(prev_code_point_, code_point))
          return Finish();
        prev_code_point_ = code_point;
        boundary_offset_ += 2;
        return MoveToNextState(InternalState::kSearch);
      }
    case InternalState::kFinished:
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedFollowingCodeUnit() once it finishes.";
      return Finish();
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled staet: " << internal_state_;
  return Finish();
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::TellEndOfPrecedingText() {
  DCHECK(internal_state_ == InternalState::kCountRIS ||
         internal_state_ == InternalState::kCountRISWaitLeadSurrogate)
      << "Do not call tellEndOfPrecedingText() once "
      << TextSegmentationMachineState::kNeedFollowingCodeUnit
      << " is returned. InternalState: " << internal_state_;

  // Clear pending code unit since preceding buffer may end with lonely trail
  // surrogate. We can just ignore it since preceding buffer is only used for
  // counting preceding regional indicators.
  pending_code_unit_ = 0;
  return MoveToNextState(InternalState::kStartForward);
}

int ForwardGraphemeBoundaryStateMachine::FinalizeAndGetBoundaryOffset() {
  if (internal_state_ != InternalState::kFinished)
    FinishWithEndOfText();
  DCHECK_GE(boundary_offset_, 0);
  return boundary_offset_;
}

void ForwardGraphemeBoundaryStateMachine::Reset() {
  pending_code_unit_ = 0;
  boundary_offset_ = 0;
  preceding_ris_count_ = 0;
  prev_code_point_ = kUnsetCodePoint;
  internal_state_ = InternalState::kCountRIS;
}

TextSegmentationMachineState ForwardGraphemeBoundaryStateMachine::Finish() {
  DCHECK_NE(internal_state_, InternalState::kFinished);
  internal_state_ = InternalState::kFinished;
  return TextSegmentationMachineState::kFinished;
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::MoveToNextState(InternalState next_state) {
  DCHECK_NE(next_state, InternalState::kFinished) << "Use finish() instead";
  DCHECK_NE(next_state, internal_state_) << "Use staySameSatate() instead";
  internal_state_ = next_state;
  if (next_state == InternalState::kStartForward)
    return TextSegmentationMachineState::kNeedFollowingCodeUnit;
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

TextSegmentationMachineState
ForwardGraphemeBoundaryStateMachine::StaySameState() {
  DCHECK_EQ(internal_state_, InternalState::kSearch)
      << "Only Search can stay the same state.";
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

void ForwardGraphemeBoundaryStateMachine::FinishWithEndOfText() {
  switch (internal_state_) {
    case InternalState::kCountRIS:                   // Fallthrough
    case InternalState::kCountRISWaitLeadSurrogate:  // Fallthrough
    case InternalState::kStartForward:               // Fallthrough
      return;  // Haven't search anything to forward. Just finish.
    case InternalState::kStartForwardWaitTrailSurrgate:
      // Lonely lead surrogate.
      boundary_offset_ = 1;
      return;
    case InternalState::kSearch:                    // Fallthrough
    case InternalState::kSearchWaitTrailSurrogate:  // Fallthrough
      return;
    case InternalState::kFinished:  // Fallthrough
      NOTREACHED_IN_MIGRATION()
          << "Do not call finishWithEndOfText() once it finishes.";
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled state: " << internal_state_;
}
}  // namespace blink
