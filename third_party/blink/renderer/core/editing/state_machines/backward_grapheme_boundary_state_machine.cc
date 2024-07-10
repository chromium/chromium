// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/state_machines/backward_grapheme_boundary_state_machine.h"

#include <ostream>

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_util.h"
#include "third_party/blink/renderer/core/editing/state_machines/text_segmentation_machine_state.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

namespace {
const UChar32 kInvalidCodePoint = WTF::unicode::kMaxCodepoint + 1;
}  // namespace

#define FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)                         \
  /* Initial state */                                                        \
  V(kStart)                                                                  \
  /* Wating lead surrogate during initial state. */                          \
  V(kStartWaitLeadSurrogate)                                                 \
  /* Searching grapheme boundary. */                                         \
  V(kSearch)                                                                 \
  /* Waiting lead surrogate during searching grapheme boundary. */           \
  V(kSearchWaitLeadSurrogate)                                                \
  /* Counting preceding regional indicators. */                              \
  V(kCountRIS)                                                               \
  /* Wating lead surrogate during counting preceding regional indicators. */ \
  V(kCountRISWaitLeadSurrogate)                                              \
  /* The state machine has stopped. */                                       \
  V(kFinished)

enum class BackwardGraphemeBoundaryStateMachine::InternalState {
#define V(name) name,
  FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
};

std::ostream& operator<<(
    std::ostream& os,
    BackwardGraphemeBoundaryStateMachine::InternalState state) {
  static const char* const kTexts[] = {
#define V(name) #name,
      FOR_EACH_BACKWARD_GRAPHEME_BOUNDARY_STATE(V)
#undef V
  };
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown state value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown state value";
  return os << *it;
}

BackwardGraphemeBoundaryStateMachine::BackwardGraphemeBoundaryStateMachine()
    : next_code_point_(kInvalidCodePoint),
      internal_state_(InternalState::kStart) {}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::FeedPrecedingCodeUnit(UChar code_unit) {
  switch (internal_state_) {
    case InternalState::kStart:
      DCHECK_EQ(trail_surrogate_, 0);
      DCHECK_EQ(next_code_point_, kInvalidCodePoint);
      DCHECK_EQ(boundary_offset_, 0);
      DCHECK_EQ(preceding_ris_count_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        trail_surrogate_ = code_unit;
        return MoveToNextState(InternalState::kStartWaitLeadSurrogate);
      }
      if (U16_IS_LEAD(code_unit)) {
        // Lonely lead surrogate. Move to previous offset.
        boundary_offset_ = -1;
        return Finish();
      }
      next_code_point_ = code_unit;
      boundary_offset_ -= 1;
      return MoveToNextState(InternalState::kSearch);
    case InternalState::kStartWaitLeadSurrogate:
      DCHECK_NE(trail_surrogate_, 0);
      DCHECK_EQ(next_code_point_, kInvalidCodePoint);
      DCHECK_EQ(boundary_offset_, 0);
      DCHECK_EQ(preceding_ris_count_, 0);
      if (!U16_IS_LEAD(code_unit)) {
        // Lonely trail surrogate. Move to previous offset.
        boundary_offset_ = -1;
        return Finish();
      }
      next_code_point_ = U16_GET_SUPPLEMENTARY(code_unit, trail_surrogate_);
      boundary_offset_ = -2;
      trail_surrogate_ = 0;
      return MoveToNextState(InternalState::kSearch);
    case InternalState::kSearch:
      DCHECK_EQ(trail_surrogate_, 0);
      DCHECK_NE(next_code_point_, kInvalidCodePoint);
      DCHECK_LT(boundary_offset_, 0);
      DCHECK_EQ(preceding_ris_count_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        DCHECK_EQ(trail_surrogate_, 0);
        trail_surrogate_ = code_unit;
        return MoveToNextState(InternalState::kSearchWaitLeadSurrogate);
      }
      if (U16_IS_LEAD(code_unit))
        return Finish();  // Lonely lead surrogate.
      if (IsGraphemeBreak(code_unit, next_code_point_))
        return Finish();
      next_code_point_ = code_unit;
      boundary_offset_ -= 1;
      return StaySameState();
    case InternalState::kSearchWaitLeadSurrogate:
      DCHECK_NE(trail_surrogate_, 0);
      DCHECK_NE(next_code_point_, kInvalidCodePoint);
      DCHECK_LT(boundary_offset_, 0);
      DCHECK_EQ(preceding_ris_count_, 0);
      if (!U16_IS_LEAD(code_unit))
        return Finish();  // Lonely trail surrogate.
      {
        const UChar32 code_point =
            U16_GET_SUPPLEMENTARY(code_unit, trail_surrogate_);
        trail_surrogate_ = 0;
        if (Character::IsRegionalIndicator(next_code_point_) &&
            Character::IsRegionalIndicator(code_point)) {
          preceding_ris_count_ = 1;
          return MoveToNextState(InternalState::kCountRIS);
        }
        if (IsGraphemeBreak(code_point, next_code_point_))
          return Finish();
        next_code_point_ = code_point;
        boundary_offset_ -= 2;
        return MoveToNextState(InternalState::kSearch);
      }
    case InternalState::kCountRIS:
      DCHECK_EQ(trail_surrogate_, 0);
      DCHECK(Character::IsRegionalIndicator(next_code_point_));
      DCHECK_LT(boundary_offset_, 0);
      DCHECK_GT(preceding_ris_count_, 0);
      if (U16_IS_TRAIL(code_unit)) {
        DCHECK_EQ(trail_surrogate_, 0);
        trail_surrogate_ = code_unit;
        return MoveToNextState(InternalState::kCountRISWaitLeadSurrogate);
      }
      if (preceding_ris_count_ % 2 != 0)
        boundary_offset_ -= 2;
      return Finish();
    case InternalState::kCountRISWaitLeadSurrogate:
      DCHECK_NE(trail_surrogate_, 0);
      DCHECK(Character::IsRegionalIndicator(next_code_point_));
      DCHECK_LT(boundary_offset_, 0);
      DCHECK_GT(preceding_ris_count_, 0);
      if (U16_IS_LEAD(code_unit)) {
        DCHECK_NE(trail_surrogate_, 0);
        const UChar32 code_point =
            U16_GET_SUPPLEMENTARY(code_unit, trail_surrogate_);
        trail_surrogate_ = 0;
        if (Character::IsRegionalIndicator(code_point)) {
          ++preceding_ris_count_;
          return MoveToNextState(InternalState::kCountRIS);
        }
      }
      if (preceding_ris_count_ % 2 != 0)
        boundary_offset_ -= 2;
      return Finish();
    case InternalState::kFinished:
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedPrecedingCodeUnit() once it finishes.";
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled state: " << internal_state_;
  return Finish();
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::TellEndOfPrecedingText() {
  switch (internal_state_) {
    case InternalState::kStart:
      // Did nothing.
      DCHECK_EQ(boundary_offset_, 0);
      return Finish();
    case InternalState::kStartWaitLeadSurrogate:
      // Lonely trail surrogate. Move to before of it.
      DCHECK_EQ(boundary_offset_, 0);
      boundary_offset_ = -1;
      return Finish();
    case InternalState::kSearch:  // fallthrough
    case InternalState::kSearchWaitLeadSurrogate:
      return Finish();
    case InternalState::kCountRIS:  // fallthrough
    case InternalState::kCountRISWaitLeadSurrogate:
      DCHECK_GT(preceding_ris_count_, 0);
      if (preceding_ris_count_ % 2 != 0)
        boundary_offset_ -= 2;
      return Finish();
    case InternalState::kFinished:
      NOTREACHED_IN_MIGRATION()
          << "Do not call tellEndOfPrecedingText() once it finishes.";
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled state: " << internal_state_;
  return Finish();
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::FeedFollowingCodeUnit(UChar code_unit) {
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

int BackwardGraphemeBoundaryStateMachine::FinalizeAndGetBoundaryOffset() {
  if (internal_state_ != InternalState::kFinished)
    TellEndOfPrecedingText();
  DCHECK_LE(boundary_offset_, 0);
  return boundary_offset_;
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::MoveToNextState(
    InternalState next_state) {
  DCHECK_NE(next_state, InternalState::kFinished) << "Use finish() instead";
  DCHECK_NE(next_state, InternalState::kStart) << "Unable to move to Start";
  DCHECK_NE(internal_state_, next_state) << "Use staySameState() instead.";
  internal_state_ = next_state;
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

TextSegmentationMachineState
BackwardGraphemeBoundaryStateMachine::StaySameState() {
  DCHECK_EQ(internal_state_, InternalState::kSearch) << "Only Search can stay.";
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

TextSegmentationMachineState BackwardGraphemeBoundaryStateMachine::Finish() {
  DCHECK_NE(internal_state_, InternalState::kFinished);
  internal_state_ = InternalState::kFinished;
  return TextSegmentationMachineState::kFinished;
}

void BackwardGraphemeBoundaryStateMachine::Reset() {
  trail_surrogate_ = 0;
  next_code_point_ = kInvalidCodePoint;
  boundary_offset_ = 0;
  preceding_ris_count_ = 0;
  internal_state_ = InternalState::kStart;
}

}  // namespace blink
