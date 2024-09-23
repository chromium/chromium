// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/state_machines/backspace_state_machine.h"

#include <ostream>

#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

#define FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)                        \
  /* Initial state */                                                    \
  V(kStart)                                                              \
  /* The current offset is just before line feed. */                     \
  V(kBeforeLF)                                                           \
  /* The current offset is just before keycap. */                        \
  V(kBeforeKeycap)                                                       \
  /* The current offset is just before variation selector and keycap. */ \
  V(kBeforeVSAndKeycap)                                                  \
  /* The current offset is just before emoji modifier. */                \
  V(kBeforeEmojiModifier)                                                \
  /* The current offset is just before variation selector and emoji*/    \
  /* modifier. */                                                        \
  V(kBeforeVSAndEmojiModifier)                                           \
  /* The current offset is just before variation sequence. */            \
  V(kBeforeVS)                                                           \
  /* The current offset is just before ZWJ emoji. */                     \
  V(kBeforeZWJEmoji)                                                     \
  /* The current offset is just before ZWJ. */                           \
  V(kBeforeZWJ)                                                          \
  /* The current offset is just before variation selector and ZWJ. */    \
  V(kBeforeVSAndZWJ)                                                     \
  /* That there are odd numbered RIS from the beggining. */              \
  V(kOddNumberedRIS)                                                     \
  /* That there are even numbered RIS from the begging. */               \
  V(kEvenNumberedRIS)                                                    \
  /* This state machine has finished. */                                 \
  V(kFinished)

enum class BackspaceStateMachine::BackspaceState {
#define V(name) name,
  FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
};

std::ostream& operator<<(std::ostream& os,
                         BackspaceStateMachine::BackspaceState state) {
  static const char* const kTexts[] = {
#define V(name) #name,
      FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
  };
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(state);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown backspace value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown backspace value";
  return os << *it;
}

BackspaceStateMachine::BackspaceStateMachine()
    : state_(BackspaceState::kStart) {}

TextSegmentationMachineState BackspaceStateMachine::FeedPrecedingCodeUnit(
    UChar code_unit) {
  DCHECK_NE(BackspaceState::kFinished, state_);
  uint32_t code_point = code_unit;
  if (U16_IS_LEAD(code_unit)) {
    if (trail_surrogate_ == 0) {
      // Unpaired lead surrogate. Aborting with deleting broken surrogate.
      ++code_units_to_be_deleted_;
      return TextSegmentationMachineState::kFinished;
    }
    code_point = U16_GET_SUPPLEMENTARY(code_unit, trail_surrogate_);
    trail_surrogate_ = 0;
  } else if (U16_IS_TRAIL(code_unit)) {
    if (trail_surrogate_ != 0) {
      // Unpaired trail surrogate. Aborting with deleting broken
      // surrogate.
      return TextSegmentationMachineState::kFinished;
    }
    trail_surrogate_ = code_unit;
    return TextSegmentationMachineState::kNeedMoreCodeUnit;
  } else {
    if (trail_surrogate_ != 0) {
      // Unpaired trail surrogate. Aborting with deleting broken
      // surrogate.
      return TextSegmentationMachineState::kFinished;
    }
  }

  switch (state_) {
    case BackspaceState::kStart:
      code_units_to_be_deleted_ = U16_LENGTH(code_point);
      if (code_point == kNewlineCharacter)
        return MoveToNextState(BackspaceState::kBeforeLF);
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR))
        return MoveToNextState(BackspaceState::kBeforeVS);
      if (Character::IsRegionalIndicator(code_point))
        return MoveToNextState(BackspaceState::kOddNumberedRIS);
      if (Character::IsModifier(code_point))
        return MoveToNextState(BackspaceState::kBeforeEmojiModifier);
      if (Character::IsEmoji(code_point))
        return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      if (code_point == kCombiningEnclosingKeycapCharacter)
        return MoveToNextState(BackspaceState::kBeforeKeycap);
      return Finish();
    case BackspaceState::kBeforeLF:
      if (code_point == kCarriageReturnCharacter)
        ++code_units_to_be_deleted_;
      return Finish();
    case BackspaceState::kBeforeKeycap:
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(last_seen_vs_code_units_, 0);
        last_seen_vs_code_units_ = U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kBeforeVSAndKeycap);
      }
      if (Character::IsEmojiKeycapBase(code_point))
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
      return Finish();
    case BackspaceState::kBeforeVSAndKeycap:
      if (Character::IsEmojiKeycapBase(code_point)) {
        DCHECK_GT(last_seen_vs_code_units_, 0);
        DCHECK_LE(last_seen_vs_code_units_, 2);
        code_units_to_be_deleted_ +=
            last_seen_vs_code_units_ + U16_LENGTH(code_point);
      }
      return Finish();
    case BackspaceState::kBeforeEmojiModifier:
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(last_seen_vs_code_units_, 0);
        last_seen_vs_code_units_ = U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kBeforeVSAndEmojiModifier);
      }
      if (Character::IsEmojiModifierBase(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      }
      return Finish();
    case BackspaceState::kBeforeVSAndEmojiModifier:
      if (Character::IsEmojiModifierBase(code_point)) {
        DCHECK_GT(last_seen_vs_code_units_, 0);
        DCHECK_LE(last_seen_vs_code_units_, 2);
        code_units_to_be_deleted_ +=
            last_seen_vs_code_units_ + U16_LENGTH(code_point);
      }
      return Finish();
    case BackspaceState::kBeforeVS:
      if (Character::IsEmoji(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      }
      if (!u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR) &&
          u_getCombiningClass(code_point) == 0)
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
      return Finish();
    case BackspaceState::kBeforeZWJEmoji:
      return code_point == kZeroWidthJoinerCharacter
                 ? MoveToNextState(BackspaceState::kBeforeZWJ)
                 : Finish();
    case BackspaceState::kBeforeZWJ:
      if (Character::IsEmoji(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point) + 1;  // +1 for ZWJ
        return Character::IsModifier(code_point)
                   ? MoveToNextState(BackspaceState::kBeforeEmojiModifier)
                   : MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      }
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR)) {
        DCHECK_EQ(last_seen_vs_code_units_, 0);
        last_seen_vs_code_units_ = U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kBeforeVSAndZWJ);
      }
      return Finish();
    case BackspaceState::kBeforeVSAndZWJ:
      if (!Character::IsEmoji(code_point))
        return Finish();

      DCHECK_GT(last_seen_vs_code_units_, 0);
      DCHECK_LE(last_seen_vs_code_units_, 2);
      // +1 for ZWJ
      code_units_to_be_deleted_ +=
          U16_LENGTH(code_point) + 1 + last_seen_vs_code_units_;
      last_seen_vs_code_units_ = 0;
      return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
    case BackspaceState::kOddNumberedRIS:
      if (!Character::IsRegionalIndicator(code_point))
        return Finish();
      code_units_to_be_deleted_ += 2;  // Code units of RIS
      return MoveToNextState(BackspaceState::kEvenNumberedRIS);
    case BackspaceState::kEvenNumberedRIS:
      if (!Character::IsRegionalIndicator(code_point))
        return Finish();
      code_units_to_be_deleted_ -= 2;  // Code units of RIS
      return MoveToNextState(BackspaceState::kOddNumberedRIS);
    case BackspaceState::kFinished:
      NOTREACHED_IN_MIGRATION()
          << "Do not call feedPrecedingCodeUnit() once it finishes.";
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled state: " << state_;
  }
  NOTREACHED_IN_MIGRATION() << "Unhandled state: " << state_;
  return TextSegmentationMachineState::kInvalid;
}

TextSegmentationMachineState BackspaceStateMachine::TellEndOfPrecedingText() {
  if (trail_surrogate_ != 0) {
    // Unpaired trail surrogate. Removing broken surrogate.
    ++code_units_to_be_deleted_;
    trail_surrogate_ = 0;
  }
  return TextSegmentationMachineState::kFinished;
}

TextSegmentationMachineState BackspaceStateMachine::FeedFollowingCodeUnit(
    UChar code_unit) {
  NOTREACHED_IN_MIGRATION();
  return TextSegmentationMachineState::kInvalid;
}

int BackspaceStateMachine::FinalizeAndGetBoundaryOffset() {
  if (trail_surrogate_ != 0) {
    // Unpaired trail surrogate. Removing broken surrogate.
    ++code_units_to_be_deleted_;
    trail_surrogate_ = 0;
  }
  if (state_ != BackspaceState::kFinished) {
    last_seen_vs_code_units_ = 0;
    state_ = BackspaceState::kFinished;
  }
  return -code_units_to_be_deleted_;
}

void BackspaceStateMachine::Reset() {
  code_units_to_be_deleted_ = 0;
  trail_surrogate_ = 0;
  state_ = BackspaceState::kStart;
  last_seen_vs_code_units_ = 0;
}

TextSegmentationMachineState BackspaceStateMachine::MoveToNextState(
    BackspaceState new_state) {
  DCHECK_NE(BackspaceState::kFinished, new_state) << "Use finish() instead.";
  DCHECK_NE(BackspaceState::kStart, new_state) << "Don't move to Start.";
  // Below |DCHECK_NE()| prevent us to infinite loop in state machine.
  DCHECK_NE(state_, new_state) << "State should be changed.";
  state_ = new_state;
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

TextSegmentationMachineState BackspaceStateMachine::Finish() {
  DCHECK_NE(BackspaceState::kFinished, state_);
  state_ = BackspaceState::kFinished;
  return TextSegmentationMachineState::kFinished;
}

}  // namespace blink
