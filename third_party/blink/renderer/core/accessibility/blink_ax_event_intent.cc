// Copyright 2020 The Chromium Authors
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
  ax::mojom::blink::InputEventType input_event_type;
  switch (edit_command.GetInputType()) {
    case InputEvent::InputType::kNone:
      return BlinkAXEventIntent();  // An empty intent.

    // Insertion.
    case InputEvent::InputType::kInsertText:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertText;
      break;
    case InputEvent::InputType::kInsertLineBreak:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertLineBreak;
      break;
    case InputEvent::InputType::kInsertParagraph:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertParagraph;
      break;
    case InputEvent::InputType::kInsertOrderedList:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertOrderedList;
      break;
    case InputEvent::InputType::kInsertUnorderedList:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertUnorderedList;
      break;
    case InputEvent::InputType::kInsertHorizontalRule:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type =
          ax::mojom::blink::InputEventType::kInsertHorizontalRule;
      break;
    case InputEvent::InputType::kInsertFromPaste:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertFromPaste;
      break;
    case InputEvent::InputType::kInsertFromDrop:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertFromDrop;
      break;
    case InputEvent::InputType::kInsertFromYank:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertFromYank;
      break;
    case InputEvent::InputType::kInsertTranspose:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertTranspose;
      break;
    case InputEvent::InputType::kInsertReplacementText:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type =
          ax::mojom::blink::InputEventType::kInsertReplacementText;
      break;
    case InputEvent::InputType::kInsertCompositionText:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type =
          ax::mojom::blink::InputEventType::kInsertCompositionText;
      break;
    case InputEvent::InputType::kInsertLink:
      command = ax::mojom::blink::Command::kInsert;
      input_event_type = ax::mojom::blink::InputEventType::kInsertLink;
      break;

    // Deletion.
    case InputEvent::InputType::kDeleteWordBackward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type = ax::mojom::blink::InputEventType::kDeleteWordBackward;
      break;
    case InputEvent::InputType::kDeleteWordForward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type = ax::mojom::blink::InputEventType::kDeleteWordForward;
      break;
    case InputEvent::InputType::kDeleteSoftLineBackward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteSoftLineBackward;
      break;
    case InputEvent::InputType::kDeleteSoftLineForward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteSoftLineForward;
      break;
    case InputEvent::InputType::kDeleteHardLineBackward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteHardLineBackward;
      break;
    case InputEvent::InputType::kDeleteHardLineForward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteHardLineForward;
      break;
    case InputEvent::InputType::kDeleteContentBackward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteContentBackward;
      break;
    case InputEvent::InputType::kDeleteContentForward:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type =
          ax::mojom::blink::InputEventType::kDeleteContentForward;
      break;
    case InputEvent::InputType::kDeleteByCut:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type = ax::mojom::blink::InputEventType::kDeleteByCut;
      break;
    case InputEvent::InputType::kDeleteByDrag:
      command = ax::mojom::blink::Command::kDelete;
      input_event_type = ax::mojom::blink::InputEventType::kDeleteByDrag;
      break;

    // History.
    case InputEvent::InputType::kHistoryUndo:
      command = ax::mojom::blink::Command::kHistory;
      input_event_type = ax::mojom::blink::InputEventType::kHistoryUndo;
      break;
    case InputEvent::InputType::kHistoryRedo:
      command = ax::mojom::blink::Command::kHistory;
      input_event_type = ax::mojom::blink::InputEventType::kHistoryRedo;
      break;

    // Formatting.
    case InputEvent::InputType::kFormatBold:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatBold;
      break;
    case InputEvent::InputType::kFormatItalic:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatItalic;
      break;
    case InputEvent::InputType::kFormatUnderline:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatUnderline;
      break;
    case InputEvent::InputType::kFormatStrikeThrough:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatStrikeThrough;
      break;
    case InputEvent::InputType::kFormatSuperscript:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatSuperscript;
      break;
    case InputEvent::InputType::kFormatSubscript:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatSubscript;
      break;
    case InputEvent::InputType::kFormatJustifyCenter:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatJustifyCenter;
      break;
    case InputEvent::InputType::kFormatJustifyFull:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatJustifyFull;
      break;
    case InputEvent::InputType::kFormatJustifyRight:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatJustifyRight;
      break;
    case InputEvent::InputType::kFormatJustifyLeft:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatJustifyLeft;
      break;
    case InputEvent::InputType::kFormatIndent:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatIndent;
      break;
    case InputEvent::InputType::kFormatOutdent:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatOutdent;
      break;
    case InputEvent::InputType::kFormatRemove:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type = ax::mojom::blink::InputEventType::kFormatRemove;
      break;
    case InputEvent::InputType::kFormatSetBlockTextDirection:
      command = ax::mojom::blink::Command::kFormat;
      input_event_type =
          ax::mojom::blink::InputEventType::kFormatSetBlockTextDirection;
      break;

    case InputEvent::InputType::kNumberOfInputTypes:
      NOTREACHED_IN_MIGRATION()
          << "Should never be assigned as an input type to |edit_command|.";
      return BlinkAXEventIntent();
  }

  return BlinkAXEventIntent(command, input_event_type);
}

// static
BlinkAXEventIntent BlinkAXEventIntent::FromClearedSelection(
    const SetSelectionBy set_selection_by) {
  // text boundary and move direction are not needed in this case.
  return BlinkAXEventIntent(ax::mojom::blink::Command::kClearSelection);
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
        case ax::mojom::blink::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return BlinkAXEventIntent();
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
        case ax::mojom::blink::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return BlinkAXEventIntent();
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
        case ax::mojom::blink::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return BlinkAXEventIntent();
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
        case ax::mojom::blink::MoveDirection::kNone:
          NOTREACHED_IN_MIGRATION();
          return BlinkAXEventIntent();
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

BlinkAXEventIntent::BlinkAXEventIntent() = default;

BlinkAXEventIntent::BlinkAXEventIntent(ax::mojom::blink::Command command)
    : intent_(command), is_initialized_(true) {}

BlinkAXEventIntent::BlinkAXEventIntent(
    ax::mojom::blink::Command command,
    ax::mojom::blink::InputEventType input_event_type)
    : intent_(command, input_event_type), is_initialized_(true) {}

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
  return BlinkAXEventIntentHashTraits::GetHash(a) ==
         BlinkAXEventIntentHashTraits::GetHash(b);
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
unsigned int BlinkAXEventIntentHashTraits::GetHash(
    const BlinkAXEventIntent& key) {
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
                    static_cast<const unsigned>(key.intent().input_event_type));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().text_boundary));
  WTF::AddIntToHash(hash,
                    static_cast<const unsigned>(key.intent().move_direction));
  return hash;
}

}  // namespace blink
