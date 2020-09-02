// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/blink_ax_event_intent.h"

#include <limits>

#include "third_party/blink/renderer/core/events/input_event.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

// static
BlinkAXEventIntent BlinkAXEventIntent::FromEditCommand(
    const EditCommand& edit_command) {
  ax::mojom::blink::Command command;
  // Set default values for move direction and text boundary.
  ax::mojom::blink::TextBoundary text_boundary =
      ax::mojom::blink::TextBoundary::kCharacter;
  ax::mojom::blink::MoveDirection move_direction =
      ax::mojom::blink::MoveDirection::kForward;

  switch (edit_command.GetInputType()) {
    case InputEvent::InputType::kNone:
      return BlinkAXEventIntent();  // An empty intent.

    // Insertion.
    case InputEvent::InputType::kInsertText:
      command = ax::mojom::blink::Command::kType;
      break;
    case InputEvent::InputType::kInsertLineBreak:
      command = ax::mojom::blink::Command::kType;
      text_boundary = ax::mojom::blink::TextBoundary::kLineEnd;
      break;
    case InputEvent::InputType::kInsertParagraph:
      command = ax::mojom::blink::Command::kType;
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphEnd;
      break;
    case InputEvent::InputType::kInsertOrderedList:
    case InputEvent::InputType::kInsertUnorderedList:
      command = ax::mojom::blink::Command::kFormat;
      break;
    case InputEvent::InputType::kInsertHorizontalRule:
      command = ax::mojom::blink::Command::kType;
      break;
    case InputEvent::InputType::kInsertFromPaste:
    case InputEvent::InputType::kInsertFromDrop:
    case InputEvent::InputType::kInsertFromYank:
      command = ax::mojom::blink::Command::kPaste;
      break;
    case InputEvent::InputType::kInsertTranspose:
    case InputEvent::InputType::kInsertReplacementText:
      command = ax::mojom::blink::Command::kReplace;
      break;
    case InputEvent::InputType::kInsertCompositionText:
      command = ax::mojom::blink::Command::kType;
      break;

    // Deletion.
    //
    // Text boundary indicates up to which point the deletion is applied. For
    // example, if a soft line break is deleted in the forward direction, then
    // it means that we are deleting until the next line start.
    case InputEvent::InputType::kDeleteWordBackward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kWordStart;
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case InputEvent::InputType::kDeleteWordForward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kWordEnd;
      break;
    case InputEvent::InputType::kDeleteSoftLineBackward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kLineEnd;
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case InputEvent::InputType::kDeleteSoftLineForward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kLineStart;
      break;
    case InputEvent::InputType::kDeleteHardLineBackward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphEnd;
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case InputEvent::InputType::kDeleteHardLineForward:
      command = ax::mojom::blink::Command::kDelete;
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphStart;
      break;
    case InputEvent::InputType::kDeleteContentBackward:
      command = ax::mojom::blink::Command::kDelete;
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case InputEvent::InputType::kDeleteContentForward:
      command = ax::mojom::blink::Command::kDelete;
      break;
    case InputEvent::InputType::kDeleteByCut:
    case InputEvent::InputType::kDeleteByDrag:
      command = ax::mojom::blink::Command::kCut;
      break;

    // History.
    case InputEvent::InputType::kHistoryUndo:
    case InputEvent::InputType::kHistoryRedo:
      return BlinkAXEventIntent();  // No accessibility sideeffects for now.

    // Formatting.
    case InputEvent::InputType::kFormatBold:
    case InputEvent::InputType::kFormatItalic:
    case InputEvent::InputType::kFormatUnderline:
    case InputEvent::InputType::kFormatStrikeThrough:
    case InputEvent::InputType::kFormatSuperscript:
    case InputEvent::InputType::kFormatSubscript:
    case InputEvent::InputType::kFormatJustifyCenter:
    case InputEvent::InputType::kFormatJustifyFull:
    case InputEvent::InputType::kFormatJustifyRight:
    case InputEvent::InputType::kFormatJustifyLeft:
    case InputEvent::InputType::kFormatIndent:
    case InputEvent::InputType::kFormatOutdent:
    case InputEvent::InputType::kFormatRemove:
    case InputEvent::InputType::kFormatSetBlockTextDirection:
      command = ax::mojom::blink::Command::kFormat;
      break;

    case InputEvent::InputType::kNumberOfInputTypes:
      NOTREACHED() << "Should never be assigned as an input type.";
      command = ax::mojom::blink::Command::kType;
      break;
  }

  return BlinkAXEventIntent(command, text_boundary, move_direction);
}

// static
BlinkAXEventIntent BlinkAXEventIntent::FromClearedSelection(
    const SetSelectionBy set_selection_by) {
  // |text_boundary| and |move_direction| are not used in this case.
  return BlinkAXEventIntent(ax::mojom::blink::Command::kClearSelection,
                            ax::mojom::blink::TextBoundary::kCharacter,
                            ax::mojom::blink::MoveDirection::kForward);
}

// static
BlinkAXEventIntent BlinkAXEventIntent::FromModifiedSelection(
    const SelectionModifyAlteration alter,
    const SelectionModifyDirection direction,
    const TextGranularity granularity,
    const SetSelectionBy set_selection_by,
    const TextDirection direction_of_selection,
    const PlatformWordBehavior platform_word_behavior) {
  ax::mojom::blink::Command command;
  switch (alter) {
    case SelectionModifyAlteration::kExtend:
      // Includes the case when the existing selection has been shrunk.
      command = ax::mojom::blink::Command::kExtendSelection;
      break;
    case SelectionModifyAlteration::kMove:
      // The existing selection has been move by a specific |granularity|, e.g.
      // the caret has been moved to the beginning of the next word.
      command = ax::mojom::blink::Command::kMoveSelection;
      break;
  }

  ax::mojom::blink::MoveDirection move_direction;
  switch (direction) {
    case SelectionModifyDirection::kBackward:
      move_direction = ax::mojom::blink::MoveDirection::kBackward;
      break;
    case SelectionModifyDirection::kForward:
      move_direction = ax::mojom::blink::MoveDirection::kForward;
      break;
    case SelectionModifyDirection::kLeft:
      move_direction = IsLtr(direction_of_selection)
                           ? ax::mojom::blink::MoveDirection::kBackward
                           : ax::mojom::blink::MoveDirection::kForward;
      break;
    case SelectionModifyDirection::kRight:
      move_direction = IsLtr(direction_of_selection)
                           ? ax::mojom::blink::MoveDirection::kForward
                           : ax::mojom::blink::MoveDirection::kBackward;
      break;
  }

  ax::mojom::blink::TextBoundary text_boundary;
  switch (granularity) {
    case TextGranularity::kCharacter:
      text_boundary = ax::mojom::blink::TextBoundary::kCharacter;
      break;
    case TextGranularity::kWord:
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          // All platforms behave the same when moving backward by word.
          text_boundary = ax::mojom::blink::TextBoundary::kWordStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          switch (platform_word_behavior) {
            case PlatformWordBehavior::kWordSkipSpaces:
              // Windows behavior is to always move to the beginning of the next
              // word.
              text_boundary = ax::mojom::blink::TextBoundary::kWordStart;
              break;
            case PlatformWordBehavior::kWordDontSkipSpaces:
              // Mac, Linux and ChromeOS behavior is to move to the end of the
              // current word.
              text_boundary = ax::mojom::blink::TextBoundary::kWordEnd;
              break;
          }
          break;
      }
      break;
    case TextGranularity::kSentence:
      // This granularity always moves to the start of the next or previous
      // sentence.
      text_boundary = ax::mojom::blink::TextBoundary::kSentenceStart;
      break;
    case TextGranularity::kLine:
      // This granularity always moves to the start of the next or previous
      // line.
      text_boundary = ax::mojom::blink::TextBoundary::kLineStart;
      break;
    case TextGranularity::kParagraph:
      // This granularity always moves to the start of the next or previous
      // paragraph.
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphStart;
      break;
    case TextGranularity::kSentenceBoundary:
      // This granularity moves either to the start or the end of the current
      // sentence, depending on the direction.
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kSentenceStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kSentenceEnd;
          break;
      }
      break;
    case TextGranularity::kLineBoundary:
      // This granularity moves either to the start or the end of the current
      // line, depending on the direction.
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kLineStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kLineEnd;
          break;
      }
      break;
    case TextGranularity::kParagraphBoundary:
      // This granularity moves either to the start or the end of the current
      // paragraph, depending on the direction.
      switch (move_direction) {
        case ax::mojom::blink::MoveDirection::kBackward:
          text_boundary = ax::mojom::blink::TextBoundary::kParagraphStart;
          break;
        case ax::mojom::blink::MoveDirection::kForward:
          text_boundary = ax::mojom::blink::TextBoundary::kParagraphEnd;
          break;
      }
      break;
    case TextGranularity::kDocumentBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kWebPage;
      break;
  }

  return BlinkAXEventIntent(command, text_boundary, move_direction);
}

// static
BlinkAXEventIntent BlinkAXEventIntent::FromNewSelection(
    const TextGranularity granularity,
    bool is_base_first,
    const SetSelectionBy set_selection_by) {
  // Unfortunately, when setting a completely new selection, |text_boundary| is
  // not always known, or is hard to compute. For example, if a new selection
  // has been made using the mouse, it would be expensive to compute any
  // meaningful granularity information.
  ax::mojom::blink::TextBoundary text_boundary;
  switch (granularity) {
    case TextGranularity::kCharacter:
      text_boundary = ax::mojom::blink::TextBoundary::kCharacter;
      break;
    case TextGranularity::kWord:
      text_boundary = ax::mojom::blink::TextBoundary::kWordStartOrEnd;
      break;
    case TextGranularity::kSentence:
    case TextGranularity::kSentenceBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kSentenceStartOrEnd;
      break;
    case TextGranularity::kLine:
    case TextGranularity::kLineBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kLineStartOrEnd;
      break;
    case TextGranularity::kParagraph:
    case TextGranularity::kParagraphBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kParagraphStartOrEnd;
      break;
    case TextGranularity::kDocumentBoundary:
      text_boundary = ax::mojom::blink::TextBoundary::kWebPage;
      break;
  }

  return BlinkAXEventIntent(
      ax::mojom::blink::Command::kSetSelection, text_boundary,
      is_base_first ? ax::mojom::blink::MoveDirection::kForward
                    : ax::mojom::blink::MoveDirection::kBackward);
}

// Creates an empty (uninitialized) instance.
BlinkAXEventIntent::BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(
    ax::mojom::blink::Command command,
    ax::mojom::blink::TextBoundary text_boundary,
    ax::mojom::blink::MoveDirection move_direction)
    : intent_(command, text_boundary, move_direction), is_initialized_(true) {}

BlinkAXEventIntent::BlinkAXEventIntent(WTF::HashTableDeletedValueType type)
    : is_initialized_(true), is_deleted_(true) {}

BlinkAXEventIntent::~BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(const BlinkAXEventIntent& intent) =
    default;

BlinkAXEventIntent& BlinkAXEventIntent::operator=(
    const BlinkAXEventIntent& intent) = default;

bool operator==(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return BlinkAXEventIntentHash::GetHash(a) ==
         BlinkAXEventIntentHash::GetHash(b);
}

bool operator!=(const BlinkAXEventIntent& a, const BlinkAXEventIntent& b) {
  return !(a == b);
}

bool BlinkAXEventIntent::IsHashTableDeletedValue() const {
  return is_deleted_;
}

std::string BlinkAXEventIntent::ToString() const {
  if (!is_initialized())
    return "AXEventIntent(uninitialized)";
  if (IsHashTableDeletedValue())
    return "AXEventIntent(is_deleted)";
  return intent().ToString();
}

// static
unsigned int BlinkAXEventIntentHash::GetHash(const BlinkAXEventIntent& key) {
  // If the intent is uninitialized, it is not safe to rely on the memory being
  // initialized to zero, because any uninitialized field that might be
  // accidentally added in the future will produce a potentially non-zero memory
  // value especially in the hard to control "intent_" member.
  if (!key.is_initialized())
    return 0u;
  if (key.IsHashTableDeletedValue())
    return std::numeric_limits<unsigned>::max();

  unsigned hash = 1u;
  WTF::AddIntToHash(hash, static_cast<const unsigned>(key.intent().command));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().text_boundary));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().move_direction));
  return hash;
}

// static
bool BlinkAXEventIntentHash::Equal(const BlinkAXEventIntent& a,
                                   const BlinkAXEventIntent& b) {
  return a == b;
}

}  // namespace blink
