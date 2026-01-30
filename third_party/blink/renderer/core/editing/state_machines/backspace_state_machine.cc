// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/state_machines/backspace_state_machine.h"

#include <array>
#include <ostream>

#include "third_party/blink/renderer/core/editing/state_machines/state_machine_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
  /* The current offset is just before emoji tag sequence cancel tag. */ \
  V(kBeforeTagTerm)                                                      \
  /* The current offset is inside an emoji tag sequence. */              \
  V(kInTagSequence)                                                      \
  /* This state machine has finished. */                                 \
  V(kFinished)

enum class BackspaceStateMachine::BackspaceState {
#define V(name) name,
  FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
};

std::ostream& operator<<(std::ostream& os,
                         BackspaceStateMachine::BackspaceState state) {
  static const auto kTexts = std::to_array<const char*>({
#define V(name) #name,
      FOR_EACH_BACKSPACE_STATE_MACHINE_STATE(V)
#undef V
  });
  DCHECK_LT(static_cast<size_t>(state), kTexts.size())
      << "Unknown backspace value";
  return os << kTexts[static_cast<size_t>(state)];
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
      if (code_point == uchar::kLineFeed) {
        return MoveToNextState(BackspaceState::kBeforeLF);
      }
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR))
        return MoveToNextState(BackspaceState::kBeforeVS);
      if (Character::IsRegionalIndicator(code_point))
        return MoveToNextState(BackspaceState::kOddNumberedRIS);
      if (Character::IsModifier(code_point))
        return MoveToNextState(BackspaceState::kBeforeEmojiModifier);
      if (IsExtendedPictographicGb11(code_point)) {
        return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      }
      if (code_point == uchar::kCombiningEnclosingKeycap) {
        return MoveToNextState(BackspaceState::kBeforeKeycap);
      }
      // Emoji tag sequences end with CANCEL TAG (U+E007F).
      // http://www.unicode.org/reports/tr51/#def_emoji_tag_sequence
      if (RuntimeEnabledFeatures::EditEmojiTagSequenceEnabled() &&
          code_point == uchar::kCancelTag) {
        return MoveToNextState(BackspaceState::kBeforeTagTerm);
      }
      return Finish();
    case BackspaceState::kBeforeLF:
      if (code_point == uchar::kCarriageReturn) {
        ++code_units_to_be_deleted_;
      }
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
        // If processing tag sequence base, finish here instead of looking
        // for ZWJ sequences.
        if (RuntimeEnabledFeatures::EditEmojiTagSequenceEnabled() &&
            processing_tag_sequence_base_) {
          processing_tag_sequence_base_ = false;
          return Finish();
        }
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
      if (IsExtendedPictographicGb11(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        // If processing tag sequence base, finish here instead of looking
        // for ZWJ sequences.
        if (RuntimeEnabledFeatures::EditEmojiTagSequenceEnabled() &&
            processing_tag_sequence_base_) {
          processing_tag_sequence_base_ = false;
          return Finish();
        }
        return MoveToNextState(BackspaceState::kBeforeZWJEmoji);
      }
      if (!u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR) &&
          u_getCombiningClass(code_point) == 0)
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
      return Finish();
    case BackspaceState::kBeforeZWJEmoji:
      return code_point == uchar::kZeroWidthJoiner
                 ? MoveToNextState(BackspaceState::kBeforeZWJ)
                 : Finish();
    case BackspaceState::kBeforeZWJ:
      if (IsExtendedPictographicGb11(code_point)) {
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
      if (!IsExtendedPictographicGb11(code_point)) {
        return Finish();
      }

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
    case BackspaceState::kBeforeTagTerm:
      // After seeing CANCEL TAG, we expect tag sequence characters.
      // http://www.unicode.org/reports/tr51/#def_emoji_tag_sequence
      if (Character::IsEmojiTagSequence(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        return MoveToNextState(BackspaceState::kInTagSequence);
      }
      // Invalid sequence - just delete the CANCEL TAG.
      return Finish();
    case BackspaceState::kInTagSequence:
      // Continue accumulating tag sequence characters.
      if (Character::IsEmojiTagSequence(code_point)) {
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        return StayInSameState();
      }
      // Found the tag_base. Per UTS #51 ED-14a, tag_base can be:
      // - emoji_character
      // - emoji_modifier_sequence
      // - emoji_presentation_sequence
      // For emoji_character, include it and finish. For modifier/presentation
      // sequences, we continue to look for the base but should finish
      // immediately after finding it (not look for ZWJ sequences).
      // https://unicode.org/reports/tr51/#def_emoji_tag_sequence
      if (Character::IsModifier(code_point)) {
        // Part of emoji_modifier_sequence - continue to find the base.
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        processing_tag_sequence_base_ = true;
        return MoveToNextState(BackspaceState::kBeforeEmojiModifier);
      }
      if (u_hasBinaryProperty(code_point, UCHAR_VARIATION_SELECTOR)) {
        // Part of emoji_presentation_sequence - continue to find the base.
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        processing_tag_sequence_base_ = true;
        return MoveToNextState(BackspaceState::kBeforeVS);
      }
      if (IsExtendedPictographicGb11(code_point)) {
        // Found an emoji_character as tag_base, include it and finish.
        code_units_to_be_deleted_ += U16_LENGTH(code_point);
        return Finish();
      }
      // Invalid sequence - stop here.
      return Finish();
    case BackspaceState::kFinished:
      NOTREACHED() << "Do not call feedPrecedingCodeUnit() once it finishes.";
    default:
      NOTREACHED() << "Unhandled state: " << state_;
  }
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
  NOTREACHED();
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
  processing_tag_sequence_base_ = false;
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

TextSegmentationMachineState BackspaceStateMachine::StayInSameState() {
  DCHECK_EQ(BackspaceState::kInTagSequence, state_)
      << "Only kInTagSequence can stay.";
  return TextSegmentationMachineState::kNeedMoreCodeUnit;
}

TextSegmentationMachineState BackspaceStateMachine::Finish() {
  DCHECK_NE(BackspaceState::kFinished, state_);
  state_ = BackspaceState::kFinished;
  return TextSegmentationMachineState::kFinished;
}

}  // namespace blink
