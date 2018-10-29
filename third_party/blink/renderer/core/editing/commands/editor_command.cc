/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/commands/editor_command.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_editing_command_type.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/editing/commands/clipboard_commands.h"
#include "third_party/blink/renderer/core/editing/commands/create_link_command.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command_names.h"
#include "third_party/blink/renderer/core/editing/commands/format_block_command.h"
#include "third_party/blink/renderer/core/editing/commands/indent_outdent_command.h"
#include "third_party/blink/renderer/core/editing/commands/insert_commands.h"
#include "third_party/blink/renderer/core/editing/commands/move_commands.h"
#include "third_party/blink/renderer/core/editing/commands/remove_format_command.h"
#include "third_party/blink/renderer/core/editing/commands/style_commands.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/commands/unlink_command.h"
#include "third_party/blink/renderer/core/editing/editing_tri_state.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/kill_ring.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#include <iterator>

namespace blink {

using namespace HTMLNames;

namespace {

struct CommandNameEntry {
  const char* name;
  WebEditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) {#name, WebEditingCommandType::k##name},
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Handles all commands except WebEditingCommandType::Invalid.
static_assert(
    arraysize(kCommandNameEntries) + 1 ==
        static_cast<size_t>(WebEditingCommandType::kNumberOfCommandTypes),
    "must handle all valid WebEditingCommandType");

WebEditingCommandType WebEditingCommandTypeFromCommandName(
    const String& command_name) {
  const CommandNameEntry* result = std::lower_bound(
      std::begin(kCommandNameEntries), std::end(kCommandNameEntries),
      command_name, [](const CommandNameEntry& entry, const String& needle) {
        return CodePointCompareIgnoringASCIICase(needle, entry.name) > 0;
      });
  if (result != std::end(kCommandNameEntries) &&
      CodePointCompareIgnoringASCIICase(command_name, result->name) == 0)
    return result->type;
  return WebEditingCommandType::kInvalid;
}

// |frame| is only used for |InsertNewline| due to how |executeInsertNewline()|
// works.
InputEvent::InputType InputTypeFromCommandType(
    WebEditingCommandType command_type,
    LocalFrame& frame) {
  // We only handle InputType on spec for 'beforeinput'.
  // http://w3c.github.io/editing/input-events.html
  using CommandType = WebEditingCommandType;
  using InputType = InputEvent::InputType;

  // |executeInsertNewline()| could do two things but we have no other ways to
  // predict.
  if (command_type == CommandType::kInsertNewline)
    return frame.GetEditor().CanEditRichly() ? InputType::kInsertParagraph
                                             : InputType::kInsertLineBreak;

  switch (command_type) {
    // Insertion.
    case CommandType::kInsertBacktab:
    case CommandType::kInsertText:
      return InputType::kInsertText;
    case CommandType::kInsertLineBreak:
      return InputType::kInsertLineBreak;
    case CommandType::kInsertParagraph:
    case CommandType::kInsertNewlineInQuotedContent:
      return InputType::kInsertParagraph;
    case CommandType::kInsertHorizontalRule:
      return InputType::kInsertHorizontalRule;
    case CommandType::kInsertOrderedList:
      return InputType::kInsertOrderedList;
    case CommandType::kInsertUnorderedList:
      return InputType::kInsertUnorderedList;

    // Deletion.
    case CommandType::kDelete:
    case CommandType::kDeleteBackward:
    case CommandType::kDeleteBackwardByDecomposingPreviousCharacter:
      return InputType::kDeleteContentBackward;
    case CommandType::kDeleteForward:
      return InputType::kDeleteContentForward;
    case CommandType::kDeleteToBeginningOfLine:
      return InputType::kDeleteSoftLineBackward;
    case CommandType::kDeleteToEndOfLine:
      return InputType::kDeleteSoftLineForward;
    case CommandType::kDeleteWordBackward:
      return InputType::kDeleteWordBackward;
    case CommandType::kDeleteWordForward:
      return InputType::kDeleteWordForward;
    case CommandType::kDeleteToBeginningOfParagraph:
      return InputType::kDeleteHardLineBackward;
    case CommandType::kDeleteToEndOfParagraph:
      return InputType::kDeleteHardLineForward;
    // TODO(editing-dev): Find appreciate InputType for following commands.
    case CommandType::kDeleteToMark:
      return InputType::kNone;

    // Command.
    case CommandType::kUndo:
      return InputType::kHistoryUndo;
    case CommandType::kRedo:
      return InputType::kHistoryRedo;
    // Cut and Paste will be handled in |Editor::dispatchCPPEvent()|.

    // Styling.
    case CommandType::kBold:
    case CommandType::kToggleBold:
      return InputType::kFormatBold;
    case CommandType::kItalic:
    case CommandType::kToggleItalic:
      return InputType::kFormatItalic;
    case CommandType::kUnderline:
    case CommandType::kToggleUnderline:
      return InputType::kFormatUnderline;
    case CommandType::kStrikethrough:
      return InputType::kFormatStrikeThrough;
    case CommandType::kSuperscript:
      return InputType::kFormatSuperscript;
    case CommandType::kSubscript:
      return InputType::kFormatSubscript;
    default:
      return InputType::kNone;
  }
}

StaticRangeVector* RangesFromCurrentSelectionOrExtendCaret(
    const LocalFrame& frame,
    SelectionModifyDirection direction,
    TextGranularity granularity) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  SelectionModifier selection_modifier(
      frame, frame.Selection().GetSelectionInDOMTree());
  selection_modifier.SetSelectionIsDirectional(
      frame.Selection().IsDirectional());
  if (selection_modifier.Selection().IsCaret())
    selection_modifier.Modify(SelectionModifyAlteration::kExtend, direction,
                              granularity);
  StaticRangeVector* ranges = new StaticRangeVector;
  // We only supports single selections.
  if (selection_modifier.Selection().IsNone())
    return ranges;
  ranges->push_back(StaticRange::Create(
      FirstEphemeralRangeOf(selection_modifier.Selection())));
  return ranges;
}

EphemeralRange ComputeRangeForTranspose(LocalFrame& frame) {
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTree();
  if (!selection.IsCaret())
    return EphemeralRange();

  // Make a selection that goes back one character and forward two characters.
  const VisiblePosition& caret = selection.VisibleStart();
  const VisiblePosition& next =
      IsEndOfParagraph(caret) ? caret : NextPositionOf(caret);
  const VisiblePosition& previous = PreviousPositionOf(next);
  if (next.DeepEquivalent() == previous.DeepEquivalent())
    return EphemeralRange();
  const VisiblePosition& previous_of_previous = PreviousPositionOf(previous);
  if (!InSameParagraph(next, previous_of_previous))
    return EphemeralRange();
  return MakeRange(previous_of_previous, next);
}

}  // anonymous namespace

class EditorInternalCommand {
 public:
  WebEditingCommandType command_type;
  bool (*execute)(LocalFrame&, Event*, EditorCommandSource, const String&);
  bool (*is_supported_from_dom)(LocalFrame*);
  bool (*is_enabled)(LocalFrame&, Event*, EditorCommandSource);
  EditingTriState (*state)(LocalFrame&, Event*);
  String (*value)(const EditorInternalCommand&, LocalFrame&, Event*);
  bool is_text_insertion;
  bool (*can_execute)(LocalFrame&, EditorCommandSource);
};

static const bool kNotTextInsertion = false;
static const bool kIsTextInsertion = true;

static bool ExecuteApplyParagraphStyle(LocalFrame& frame,
                                       EditorCommandSource source,
                                       InputEvent::InputType input_type,
                                       CSSPropertyID property_id,
                                       const String& property_value) {
  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kHTMLQuirksMode);
  style->SetProperty(property_id, property_value, /* important */ false,
                     frame.GetDocument()->GetSecureContextMode());
  // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a
  // good reason for that?
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding:
      frame.GetEditor().ApplyParagraphStyleToSelection(style, input_type);
      return true;
    case EditorCommandSource::kDOM:
      frame.GetEditor().ApplyParagraphStyle(style, input_type);
      return true;
  }
  NOTREACHED();
  return false;
}

bool ExpandSelectionToGranularity(LocalFrame& frame,
                                  TextGranularity granularity) {
  const VisibleSelection& selection = CreateVisibleSelectionWithGranularity(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(
              frame.Selection().ComputeVisibleSelectionInDOMTree().Base(),
              frame.Selection().ComputeVisibleSelectionInDOMTree().Extent())
          .Build(),
      granularity);
  const EphemeralRange new_range = selection.ToNormalizedEphemeralRange();
  if (new_range.IsNull())
    return false;
  if (new_range.IsCollapsed())
    return false;
  frame.Selection().SetSelection(
      SelectionInDOMTree::Builder().SetBaseAndExtent(new_range).Build(),
      SetSelectionOptions::Builder().SetShouldCloseTyping(true).Build());
  return true;
}

static bool HasChildTags(Element& element, const QualifiedName& tag_name) {
  return !element.getElementsByTagName(tag_name.LocalName())->IsEmpty();
}

static EditingTriState SelectionListState(const FrameSelection& selection,
                                          const QualifiedName& tag_name) {
  if (selection.ComputeVisibleSelectionInDOMTreeDeprecated().IsCaret()) {
    if (EnclosingElementWithTag(
            selection.ComputeVisibleSelectionInDOMTreeDeprecated().Start(),
            tag_name))
      return EditingTriState::kTrue;
  } else if (selection.ComputeVisibleSelectionInDOMTreeDeprecated().IsRange()) {
    Element* start_element = EnclosingElementWithTag(
        selection.ComputeVisibleSelectionInDOMTreeDeprecated().Start(),
        tag_name);
    Element* end_element = EnclosingElementWithTag(
        selection.ComputeVisibleSelectionInDOMTreeDeprecated().End(), tag_name);

    if (start_element && end_element && start_element == end_element) {
      // If the selected list has the different type of list as child, return
      // |FalseTriState|.
      // See http://crbug.com/385374
      if (HasChildTags(*start_element, tag_name.Matches(ulTag) ? olTag : ulTag))
        return EditingTriState::kFalse;
      return EditingTriState::kTrue;
    }
  }

  return EditingTriState::kFalse;
}

static EphemeralRange UnionEphemeralRanges(const EphemeralRange& range1,
                                           const EphemeralRange& range2) {
  const Position start_position =
      range1.StartPosition().CompareTo(range2.StartPosition()) <= 0
          ? range1.StartPosition()
          : range2.StartPosition();
  const Position end_position =
      range1.EndPosition().CompareTo(range2.EndPosition()) <= 0
          ? range1.EndPosition()
          : range2.EndPosition();
  return EphemeralRange(start_position, end_position);
}

// Execute command functions

static bool CanSmartCopyOrDelete(LocalFrame& frame) {
  return frame.GetEditor().SmartInsertDeleteEnabled() &&
         frame.Selection().Granularity() == TextGranularity::kWord;
}

static bool ExecuteCreateLink(LocalFrame& frame,
                              Event*,
                              EditorCommandSource,
                              const String& value) {
  if (value.IsEmpty())
    return false;
  DCHECK(frame.GetDocument());
  return CreateLinkCommand::Create(*frame.GetDocument(), value)->Apply();
}

static bool ExecuteDefaultParagraphSeparator(LocalFrame& frame,
                                             Event*,
                                             EditorCommandSource,
                                             const String& value) {
  if (DeprecatedEqualIgnoringCase(value, "div")) {
    frame.GetEditor().SetDefaultParagraphSeparator(
        EditorParagraphSeparator::kIsDiv);
    return true;
  }
  if (DeprecatedEqualIgnoringCase(value, "p")) {
    frame.GetEditor().SetDefaultParagraphSeparator(
        EditorParagraphSeparator::kIsP);
  }
  return true;
}

static void PerformDelete(LocalFrame& frame) {
  if (!frame.GetEditor().CanDelete())
    return;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  // |SelectedRange| requires clean layout for visible selection normalization.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  frame.GetEditor().AddToKillRing(frame.GetEditor().SelectedRange());
  // TODO(editing-dev): |Editor::performDelete()| has no direction.
  // https://github.com/w3c/editing/issues/130
  frame.GetEditor().DeleteSelectionWithSmartDelete(
      CanSmartCopyOrDelete(frame) ? DeleteMode::kSmart : DeleteMode::kSimple,
      InputEvent::InputType::kDeleteContentBackward);

  // clear the "start new kill ring sequence" setting, because it was set to
  // true when the selection was updated by deleting the range
  frame.GetEditor().SetStartNewKillRingSequence(false);
}

static bool ExecuteDelete(LocalFrame& frame,
                          Event*,
                          EditorCommandSource source,
                          const String&) {
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding: {
      // Doesn't modify the text if the current selection isn't a range.
      PerformDelete(frame);
      return true;
    }
    case EditorCommandSource::kDOM:
      // If the current selection is a caret, delete the preceding character. IE
      // performs forwardDelete, but we currently side with Firefox. Doesn't
      // scroll to make the selection visible, or modify the kill ring (this
      // time, siding with IE, not Firefox).
      DCHECK(frame.GetDocument());
      TypingCommand::DeleteKeyPressed(
          *frame.GetDocument(),
          frame.Selection().Granularity() == TextGranularity::kWord
              ? TypingCommand::kSmartDelete
              : 0);
      return true;
  }
  NOTREACHED();
  return false;
}

static bool DeleteWithDirection(LocalFrame& frame,
                                DeleteDirection direction,
                                TextGranularity granularity,
                                bool kill_ring,
                                bool is_typing_action) {
  Editor& editor = frame.GetEditor();
  if (!editor.CanEdit())
    return false;

  EditingState editing_state;
  if (frame.Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated()
          .IsRange()) {
    if (is_typing_action) {
      DCHECK(frame.GetDocument());
      TypingCommand::DeleteKeyPressed(
          *frame.GetDocument(),
          CanSmartCopyOrDelete(frame) ? TypingCommand::kSmartDelete : 0,
          granularity);
      editor.RevealSelectionAfterEditingOperation();
    } else {
      if (kill_ring)
        editor.AddToKillRing(editor.SelectedRange());
      editor.DeleteSelectionWithSmartDelete(
          CanSmartCopyOrDelete(frame) ? DeleteMode::kSmart
                                      : DeleteMode::kSimple,
          DeletionInputTypeFromTextGranularity(direction, granularity));
      // Implicitly calls revealSelectionAfterEditingOperation().
    }
  } else {
    TypingCommand::Options options = 0;
    if (CanSmartCopyOrDelete(frame))
      options |= TypingCommand::kSmartDelete;
    if (kill_ring)
      options |= TypingCommand::kKillRing;
    switch (direction) {
      case DeleteDirection::kForward:
        DCHECK(frame.GetDocument());
        TypingCommand::ForwardDeleteKeyPressed(
            *frame.GetDocument(), &editing_state, options, granularity);
        if (editing_state.IsAborted())
          return false;
        break;
      case DeleteDirection::kBackward:
        DCHECK(frame.GetDocument());
        TypingCommand::DeleteKeyPressed(*frame.GetDocument(), options,
                                        granularity);
        break;
    }
    editor.RevealSelectionAfterEditingOperation();
  }

  // FIXME: We should to move this down into deleteKeyPressed.
  // clear the "start new kill ring sequence" setting, because it was set to
  // true when the selection was updated by deleting the range
  if (kill_ring)
    editor.SetStartNewKillRingSequence(false);

  return true;
}

static bool ExecuteDeleteBackward(LocalFrame& frame,
                                  Event*,
                                  EditorCommandSource,
                                  const String&) {
  DeleteWithDirection(frame, DeleteDirection::kBackward,
                      TextGranularity::kCharacter, false, true);
  return true;
}

static bool ExecuteDeleteBackwardByDecomposingPreviousCharacter(
    LocalFrame& frame,
    Event*,
    EditorCommandSource,
    const String&) {
  DLOG(ERROR) << "DeleteBackwardByDecomposingPreviousCharacter is not "
                 "implemented, doing DeleteBackward instead";
  DeleteWithDirection(frame, DeleteDirection::kBackward,
                      TextGranularity::kCharacter, false, true);
  return true;
}

static bool ExecuteDeleteForward(LocalFrame& frame,
                                 Event*,
                                 EditorCommandSource,
                                 const String&) {
  DeleteWithDirection(frame, DeleteDirection::kForward,
                      TextGranularity::kCharacter, false, true);
  return true;
}

static bool ExecuteDeleteToBeginningOfLine(LocalFrame& frame,
                                           Event*,
                                           EditorCommandSource,
                                           const String&) {
  DeleteWithDirection(frame, DeleteDirection::kBackward,
                      TextGranularity::kLineBoundary, true, false);
  return true;
}

static bool ExecuteDeleteToBeginningOfParagraph(LocalFrame& frame,
                                                Event*,
                                                EditorCommandSource,
                                                const String&) {
  DeleteWithDirection(frame, DeleteDirection::kBackward,
                      TextGranularity::kParagraphBoundary, true, false);
  return true;
}

static bool ExecuteDeleteToEndOfLine(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource,
                                     const String&) {
  // Despite its name, this command should delete the newline at the end of a
  // paragraph if you are at the end of a paragraph (like
  // DeleteToEndOfParagraph).
  DeleteWithDirection(frame, DeleteDirection::kForward,
                      TextGranularity::kLineBoundary, true, false);
  return true;
}

static bool ExecuteDeleteToEndOfParagraph(LocalFrame& frame,
                                          Event*,
                                          EditorCommandSource,
                                          const String&) {
  // Despite its name, this command should delete the newline at the end of
  // a paragraph if you are at the end of a paragraph.
  DeleteWithDirection(frame, DeleteDirection::kForward,
                      TextGranularity::kParagraphBoundary, true, false);
  return true;
}

static bool ExecuteDeleteToMark(LocalFrame& frame,
                                Event*,
                                EditorCommandSource,
                                const String&) {
  const EphemeralRange mark =
      frame.GetEditor().Mark().ToNormalizedEphemeralRange();
  if (mark.IsNotNull()) {
    frame.Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .SetBaseAndExtent(
                UnionEphemeralRanges(mark, frame.GetEditor().SelectedRange()))
            .Build(),
        SetSelectionOptions::Builder().SetShouldCloseTyping(true).Build());
  }
  PerformDelete(frame);

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  frame.GetEditor().SetMark();
  return true;
}

static bool ExecuteDeleteWordBackward(LocalFrame& frame,
                                      Event*,
                                      EditorCommandSource,
                                      const String&) {
  DeleteWithDirection(frame, DeleteDirection::kBackward, TextGranularity::kWord,
                      true, false);
  return true;
}

static bool ExecuteDeleteWordForward(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource,
                                     const String&) {
  DeleteWithDirection(frame, DeleteDirection::kForward, TextGranularity::kWord,
                      true, false);
  return true;
}

static bool ExecuteFindString(LocalFrame& frame,
                              Event*,
                              EditorCommandSource,
                              const String& value) {
  return Editor::FindString(frame, value, kCaseInsensitive | kWrapAround);
}

static bool ExecuteFormatBlock(LocalFrame& frame,
                               Event*,
                               EditorCommandSource,
                               const String& value) {
  String tag_name = value.DeprecatedLower();
  if (tag_name[0] == '<' && tag_name[tag_name.length() - 1] == '>')
    tag_name = tag_name.Substring(1, tag_name.length() - 2);

  AtomicString local_name, prefix;
  if (!Document::ParseQualifiedName(AtomicString(tag_name), prefix, local_name,
                                    IGNORE_EXCEPTION_FOR_TESTING))
    return false;
  QualifiedName qualified_tag_name(prefix, local_name, xhtmlNamespaceURI);

  DCHECK(frame.GetDocument());
  FormatBlockCommand* command =
      FormatBlockCommand::Create(*frame.GetDocument(), qualified_tag_name);
  command->Apply();
  return command->DidApply();
}

static bool ExecuteForwardDelete(LocalFrame& frame,
                                 Event*,
                                 EditorCommandSource source,
                                 const String&) {
  EditingState editing_state;
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding:
      DeleteWithDirection(frame, DeleteDirection::kForward,
                          TextGranularity::kCharacter, false, true);
      return true;
    case EditorCommandSource::kDOM:
      // Doesn't scroll to make the selection visible, or modify the kill ring.
      // ForwardDelete is not implemented in IE or Firefox, so this behavior is
      // only needed for backward compatibility with ourselves, and for
      // consistency with Delete.
      DCHECK(frame.GetDocument());
      TypingCommand::ForwardDeleteKeyPressed(*frame.GetDocument(),
                                             &editing_state);
      if (editing_state.IsAborted())
        return false;
      return true;
  }
  NOTREACHED();
  return false;
}

static bool ExecuteIgnoreSpelling(LocalFrame& frame,
                                  Event*,
                                  EditorCommandSource,
                                  const String&) {
  frame.GetSpellChecker().IgnoreSpelling();
  return true;
}

static bool ExecuteIndent(LocalFrame& frame,
                          Event*,
                          EditorCommandSource,
                          const String&) {
  DCHECK(frame.GetDocument());
  return IndentOutdentCommand::Create(*frame.GetDocument(),
                                      IndentOutdentCommand::kIndent)
      ->Apply();
}

static bool ExecuteJustifyCenter(LocalFrame& frame,
                                 Event*,
                                 EditorCommandSource source,
                                 const String&) {
  return ExecuteApplyParagraphStyle(frame, source,
                                    InputEvent::InputType::kFormatJustifyCenter,
                                    CSSPropertyTextAlign, "center");
}

static bool ExecuteJustifyFull(LocalFrame& frame,
                               Event*,
                               EditorCommandSource source,
                               const String&) {
  return ExecuteApplyParagraphStyle(frame, source,
                                    InputEvent::InputType::kFormatJustifyFull,
                                    CSSPropertyTextAlign, "justify");
}

static bool ExecuteJustifyLeft(LocalFrame& frame,
                               Event*,
                               EditorCommandSource source,
                               const String&) {
  return ExecuteApplyParagraphStyle(frame, source,
                                    InputEvent::InputType::kFormatJustifyLeft,
                                    CSSPropertyTextAlign, "left");
}

static bool ExecuteJustifyRight(LocalFrame& frame,
                                Event*,
                                EditorCommandSource source,
                                const String&) {
  return ExecuteApplyParagraphStyle(frame, source,
                                    InputEvent::InputType::kFormatJustifyRight,
                                    CSSPropertyTextAlign, "right");
}

static bool ExecuteOutdent(LocalFrame& frame,
                           Event*,
                           EditorCommandSource,
                           const String&) {
  DCHECK(frame.GetDocument());
  return IndentOutdentCommand::Create(*frame.GetDocument(),
                                      IndentOutdentCommand::kOutdent)
      ->Apply();
}

static bool ExecuteToggleOverwrite(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource,
                                   const String&) {
  frame.GetEditor().ToggleOverwriteModeEnabled();
  return true;
}

static bool ExecutePrint(LocalFrame& frame,
                         Event*,
                         EditorCommandSource,
                         const String&) {
  Page* page = frame.GetPage();
  if (!page)
    return false;
  return page->GetChromeClient().Print(&frame);
}

static bool ExecuteRedo(LocalFrame& frame,
                        Event*,
                        EditorCommandSource,
                        const String&) {
  frame.GetEditor().Redo();
  return true;
}

static bool ExecuteRemoveFormat(LocalFrame& frame,
                                Event*,
                                EditorCommandSource,
                                const String&) {
  DCHECK(frame.GetDocument());
  RemoveFormatCommand::Create(*frame.GetDocument())->Apply();

  return true;
}

static bool ExecuteScrollPageBackward(LocalFrame& frame,
                                      Event*,
                                      EditorCommandSource,
                                      const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollBlockDirectionBackward,
                                                kScrollByPage);
}

static bool ExecuteScrollPageForward(LocalFrame& frame,
                                     Event*,
                                     EditorCommandSource,
                                     const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollBlockDirectionForward,
                                                kScrollByPage);
}

static bool ExecuteScrollLineUp(LocalFrame& frame,
                                Event*,
                                EditorCommandSource,
                                const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollUpIgnoringWritingMode,
                                                kScrollByLine);
}

static bool ExecuteScrollLineDown(LocalFrame& frame,
                                  Event*,
                                  EditorCommandSource,
                                  const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollDownIgnoringWritingMode,
                                                kScrollByLine);
}

static bool ExecuteScrollToBeginningOfDocument(LocalFrame& frame,
                                               Event*,
                                               EditorCommandSource,
                                               const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollBlockDirectionBackward,
                                                kScrollByDocument);
}

static bool ExecuteScrollToEndOfDocument(LocalFrame& frame,
                                         Event*,
                                         EditorCommandSource,
                                         const String&) {
  return frame.GetEventHandler().BubblingScroll(kScrollBlockDirectionForward,
                                                kScrollByDocument);
}

static bool ExecuteSelectAll(LocalFrame& frame,
                             Event*,
                             EditorCommandSource source,
                             const String&) {
  const SetSelectionBy set_selection_by =
      source == EditorCommandSource::kMenuOrKeyBinding
          ? SetSelectionBy::kUser
          : SetSelectionBy::kSystem;
  frame.Selection().SelectAll(set_selection_by);
  return true;
}

static bool ExecuteSelectLine(LocalFrame& frame,
                              Event*,
                              EditorCommandSource,
                              const String&) {
  return ExpandSelectionToGranularity(frame, TextGranularity::kLine);
}

static bool ExecuteSelectParagraph(LocalFrame& frame,
                                   Event*,
                                   EditorCommandSource,
                                   const String&) {
  return ExpandSelectionToGranularity(frame, TextGranularity::kParagraph);
}

static bool ExecuteSelectSentence(LocalFrame& frame,
                                  Event*,
                                  EditorCommandSource,
                                  const String&) {
  return ExpandSelectionToGranularity(frame, TextGranularity::kSentence);
}

static bool ExecuteSelectToMark(LocalFrame& frame,
                                Event*,
                                EditorCommandSource,
                                const String&) {
  const EphemeralRange mark =
      frame.GetEditor().Mark().ToNormalizedEphemeralRange();
  EphemeralRange selection = frame.GetEditor().SelectedRange();
  if (mark.IsNull() || selection.IsNull())
    return false;
  frame.Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(UnionEphemeralRanges(mark, selection))
          .Build(),
      SetSelectionOptions::Builder().SetShouldCloseTyping(true).Build());
  return true;
}

static bool ExecuteSelectWord(LocalFrame& frame,
                              Event*,
                              EditorCommandSource,
                              const String&) {
  return ExpandSelectionToGranularity(frame, TextGranularity::kWord);
}

static bool ExecuteSetMark(LocalFrame& frame,
                           Event*,
                           EditorCommandSource,
                           const String&) {
  frame.GetEditor().SetMark();
  return true;
}

static bool ExecuteSwapWithMark(LocalFrame& frame,
                                Event*,
                                EditorCommandSource,
                                const String&) {
  const VisibleSelection mark(frame.GetEditor().Mark());
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  const bool mark_is_directional = frame.GetEditor().MarkIsDirectional();
  if (mark.IsNone() || selection.IsNone())
    return false;

  frame.GetEditor().SetMark();
  frame.Selection().SetSelection(mark.AsSelection(),
                                 SetSelectionOptions::Builder()
                                     .SetIsDirectional(mark_is_directional)
                                     .Build());
  return true;
}

static bool ExecuteTranspose(LocalFrame& frame,
                             Event*,
                             EditorCommandSource,
                             const String&) {
  Editor& editor = frame.GetEditor();
  if (!editor.CanEdit())
    return false;

  Document* const document = frame.GetDocument();

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document->UpdateStyleAndLayoutIgnorePendingStylesheets();

  const EphemeralRange& range = ComputeRangeForTranspose(frame);
  if (range.IsNull())
    return false;

  // Transpose the two characters.
  const String& text = PlainText(range);
  if (text.length() != 2)
    return false;
  const String& transposed = text.Right(1) + text.Left(1);

  if (DispatchBeforeInputInsertText(
          EventTargetNodeForDocument(document), transposed,
          InputEvent::InputType::kInsertTranspose,
          new StaticRangeVector(1, StaticRange::Create(range))) !=
      DispatchEventResult::kNotCanceled)
    return false;

  // 'beforeinput' event handler may destroy document->
  if (frame.GetDocument() != document)
    return false;

  // TODO(editing-dev): The use of UpdateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  document->UpdateStyleAndLayoutIgnorePendingStylesheets();

  // 'beforeinput' event handler may change selection, we need to re-calculate
  // range.
  const EphemeralRange& new_range = ComputeRangeForTranspose(frame);
  if (new_range.IsNull())
    return false;

  const String& new_text = PlainText(new_range);
  if (new_text.length() != 2)
    return false;
  const String& new_transposed = new_text.Right(1) + new_text.Left(1);

  const SelectionInDOMTree& new_selection =
      SelectionInDOMTree::Builder().SetBaseAndExtent(new_range).Build();

  // Select the two characters.
  if (CreateVisibleSelection(new_selection) !=
      frame.Selection().ComputeVisibleSelectionInDOMTree())
    frame.Selection().SetSelectionAndEndTyping(new_selection);

  // Insert the transposed characters.
  editor.ReplaceSelectionWithText(new_transposed, false, false,
                                  InputEvent::InputType::kInsertTranspose);
  return true;
}

static bool ExecuteUndo(LocalFrame& frame,
                        Event*,
                        EditorCommandSource,
                        const String&) {
  frame.GetEditor().Undo();
  return true;
}

static bool ExecuteUnlink(LocalFrame& frame,
                          Event*,
                          EditorCommandSource,
                          const String&) {
  DCHECK(frame.GetDocument());
  return UnlinkCommand::Create(*frame.GetDocument())->Apply();
}

static bool ExecuteUnselect(LocalFrame& frame,
                            Event*,
                            EditorCommandSource,
                            const String&) {
  frame.Selection().Clear();
  return true;
}

static bool ExecuteYank(LocalFrame& frame,
                        Event*,
                        EditorCommandSource,
                        const String&) {
  const String& yank_string = frame.GetEditor().GetKillRing().Yank();
  if (DispatchBeforeInputInsertText(
          EventTargetNodeForDocument(frame.GetDocument()), yank_string,
          InputEvent::InputType::kInsertFromYank) !=
      DispatchEventResult::kNotCanceled)
    return true;

  // 'beforeinput' event handler may destroy document.
  if (frame.GetDocument()->GetFrame() != &frame)
    return false;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  frame.GetEditor().InsertTextWithoutSendingTextEvent(
      yank_string, false, nullptr, InputEvent::InputType::kInsertFromYank);
  frame.GetEditor().GetKillRing().SetToYankedState();
  return true;
}

static bool ExecuteYankAndSelect(LocalFrame& frame,
                                 Event*,
                                 EditorCommandSource,
                                 const String&) {
  const String& yank_string = frame.GetEditor().GetKillRing().Yank();
  if (DispatchBeforeInputInsertText(
          EventTargetNodeForDocument(frame.GetDocument()), yank_string,
          InputEvent::InputType::kInsertFromYank) !=
      DispatchEventResult::kNotCanceled)
    return true;

  // 'beforeinput' event handler may destroy document.
  if (frame.GetDocument()->GetFrame() != &frame)
    return false;

  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  frame.GetEditor().InsertTextWithoutSendingTextEvent(
      frame.GetEditor().GetKillRing().Yank(), true, nullptr,
      InputEvent::InputType::kInsertFromYank);
  frame.GetEditor().GetKillRing().SetToYankedState();
  return true;
}

// Supported functions

static bool Supported(LocalFrame*) {
  return true;
}

static bool SupportedFromMenuOrKeyBinding(LocalFrame*) {
  return false;
}

// Enabled functions

static bool Enabled(LocalFrame&, Event*, EditorCommandSource) {
  return true;
}

static bool EnabledVisibleSelection(LocalFrame& frame,
                                    Event* event,
                                    EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;

  // The term "visible" here includes a caret in editable text or a range in any
  // text.
  const VisibleSelection& selection =
      CreateVisibleSelection(frame.GetEditor().SelectionForCommand(event));
  return (selection.IsCaret() && selection.IsContentEditable()) ||
         selection.IsRange();
}

static bool EnabledVisibleSelectionAndMark(LocalFrame& frame,
                                           Event* event,
                                           EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;

  const VisibleSelection& selection =
      CreateVisibleSelection(frame.GetEditor().SelectionForCommand(event));
  return ((selection.IsCaret() && selection.IsContentEditable()) ||
          selection.IsRange()) &&
         !frame.GetEditor().Mark().IsNone();
}

static bool EnableCaretInEditableText(LocalFrame& frame,
                                      Event* event,
                                      EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  const VisibleSelection& selection =
      CreateVisibleSelection(frame.GetEditor().SelectionForCommand(event));
  return selection.IsCaret() && selection.IsContentEditable();
}

static bool EnabledInEditableText(LocalFrame& frame,
                                  Event* event,
                                  EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  const SelectionInDOMTree selection =
      frame.GetEditor().SelectionForCommand(event);
  return RootEditableElementOf(
      CreateVisiblePosition(selection.Base()).DeepEquivalent());
}

static bool EnabledDelete(LocalFrame& frame,
                          Event* event,
                          EditorCommandSource source) {
  switch (source) {
    case EditorCommandSource::kMenuOrKeyBinding:
      return frame.Selection().SelectionHasFocus() &&
             frame.GetEditor().CanDelete();
    case EditorCommandSource::kDOM:
      // "Delete" from DOM is like delete/backspace keypress, affects selected
      // range if non-empty, otherwise removes a character
      return EnabledInEditableText(frame, event, source);
  }
  NOTREACHED();
  return false;
}

static bool EnabledInRichlyEditableText(LocalFrame& frame,
                                        Event*,
                                        EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTree();
  return !selection.IsNone() && IsRichlyEditablePosition(selection.Base()) &&
         selection.RootEditableElement();
}

static bool EnabledRangeInEditableText(LocalFrame& frame,
                                       Event*,
                                       EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  return frame.Selection()
             .ComputeVisibleSelectionInDOMTreeDeprecated()
             .IsRange() &&
         frame.Selection()
             .ComputeVisibleSelectionInDOMTreeDeprecated()
             .IsContentEditable();
}

static bool EnabledRangeInRichlyEditableText(LocalFrame& frame,
                                             Event*,
                                             EditorCommandSource source) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      !frame.Selection().SelectionHasFocus())
    return false;
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTree();
  return selection.IsRange() && IsRichlyEditablePosition(selection.Base());
}

static bool EnabledRedo(LocalFrame& frame, Event*, EditorCommandSource) {
  return frame.GetEditor().CanRedo();
}

static bool EnabledUndo(LocalFrame& frame, Event*, EditorCommandSource) {
  return frame.GetEditor().CanUndo();
}

static bool EnabledUnselect(LocalFrame& frame,
                            Event* event,
                            EditorCommandSource) {
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

  // The term "visible" here includes a caret in editable text or a range in any
  // text.
  const VisibleSelection& selection =
      CreateVisibleSelection(frame.GetEditor().SelectionForCommand(event));
  return (selection.IsCaret() && selection.IsContentEditable()) ||
         selection.IsRange();
}

static bool EnabledSelectAll(LocalFrame& frame,
                             Event*,
                             EditorCommandSource source) {
  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone())
    return true;
  // Hidden selection appears as no selection to users, in which case user-
  // triggered SelectAll should be enabled and act as if there is no selection.
  if (source == EditorCommandSource::kMenuOrKeyBinding &&
      frame.Selection().IsHidden())
    return true;
  if (Node* root = HighestEditableRoot(selection.Start())) {
    if (!root->hasChildren())
      return false;
    // TODO(amaralp): Return false if already fully selected.
  }
  // TODO(amaralp): Address user-select handling.
  return true;
}

// State functions

static EditingTriState StateNone(LocalFrame&, Event*) {
  return EditingTriState::kFalse;
}

EditingTriState StateOrderedList(LocalFrame& frame, Event*) {
  return SelectionListState(frame.Selection(), olTag);
}

static EditingTriState StateUnorderedList(LocalFrame& frame, Event*) {
  return SelectionListState(frame.Selection(), ulTag);
}

static EditingTriState StateJustifyCenter(LocalFrame& frame, Event*) {
  return StyleCommands::StateStyle(frame, CSSPropertyTextAlign, "center");
}

static EditingTriState StateJustifyFull(LocalFrame& frame, Event*) {
  return StyleCommands::StateStyle(frame, CSSPropertyTextAlign, "justify");
}

static EditingTriState StateJustifyLeft(LocalFrame& frame, Event*) {
  return StyleCommands::StateStyle(frame, CSSPropertyTextAlign, "left");
}

static EditingTriState StateJustifyRight(LocalFrame& frame, Event*) {
  return StyleCommands::StateStyle(frame, CSSPropertyTextAlign, "right");
}

// Value functions

static String ValueStateOrNull(const EditorInternalCommand& self,
                               LocalFrame& frame,
                               Event* triggering_event) {
  if (self.state == StateNone)
    return String();
  return self.state(frame, triggering_event) == EditingTriState::kTrue
             ? "true"
             : "false";
}

// The command has no value.
// https://w3c.github.io/editing/execCommand.html#querycommandvalue()
// > ... or has no value, return the empty string.
static String ValueEmpty(const EditorInternalCommand&, LocalFrame&, Event*) {
  return g_empty_string;
}

static String ValueDefaultParagraphSeparator(const EditorInternalCommand&,
                                             LocalFrame& frame,
                                             Event*) {
  switch (frame.GetEditor().DefaultParagraphSeparator()) {
    case EditorParagraphSeparator::kIsDiv:
      return divTag.LocalName();
    case EditorParagraphSeparator::kIsP:
      return pTag.LocalName();
  }

  NOTREACHED();
  return String();
}

static String ValueFormatBlock(const EditorInternalCommand&,
                               LocalFrame& frame,
                               Event*) {
  const VisibleSelection& selection =
      frame.Selection().ComputeVisibleSelectionInDOMTreeDeprecated();
  if (selection.IsNone() || !selection.IsValidFor(*(frame.GetDocument())) ||
      !selection.IsContentEditable())
    return "";
  Element* format_block_element =
      FormatBlockCommand::ElementForFormatBlockCommand(
          FirstEphemeralRangeOf(selection));
  if (!format_block_element)
    return "";
  return format_block_element->localName();
}

// CanExectue functions

static bool CanNotExecuteWhenDisabled(LocalFrame&, EditorCommandSource) {
  return false;
}

// Map of functions

static const EditorInternalCommand* InternalCommand(
    const String& command_name) {
  static const EditorInternalCommand kEditorCommands[] = {
      // Lists all commands in blink::WebEditingCommandType.
      // Must be ordered by |commandType| for index lookup.
      // Covered by unit tests in editing_command_test.cc
      {WebEditingCommandType::kAlignJustified, ExecuteJustifyFull,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kAlignLeft, ExecuteJustifyLeft,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kAlignRight, ExecuteJustifyRight,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kBackColor, StyleCommands::ExecuteBackColor,
       Supported, EnabledInRichlyEditableText, StateNone,
       StyleCommands::ValueBackColor, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      // FIXME: remove BackwardDelete when Safari for Windows stops using it.
      {WebEditingCommandType::kBackwardDelete, ExecuteDeleteBackward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kBold, StyleCommands::ExecuteToggleBold,
       Supported, EnabledInRichlyEditableText, StyleCommands::StateBold,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kCopy, ClipboardCommands::ExecuteCopy, Supported,
       ClipboardCommands::EnabledCopy, StateNone, ValueStateOrNull,
       kNotTextInsertion, ClipboardCommands::CanWriteClipboard},
      {WebEditingCommandType::kCreateLink, ExecuteCreateLink, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kCut, ClipboardCommands::ExecuteCut, Supported,
       ClipboardCommands::EnabledCut, StateNone, ValueStateOrNull,
       kNotTextInsertion, ClipboardCommands::CanWriteClipboard},
      {WebEditingCommandType::kDefaultParagraphSeparator,
       ExecuteDefaultParagraphSeparator, Supported, Enabled, StateNone,
       ValueDefaultParagraphSeparator, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDelete, ExecuteDelete, Supported, EnabledDelete,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteBackward, ExecuteDeleteBackward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteBackwardByDecomposingPreviousCharacter,
       ExecuteDeleteBackwardByDecomposingPreviousCharacter,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteForward, ExecuteDeleteForward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteToBeginningOfLine,
       ExecuteDeleteToBeginningOfLine, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteToBeginningOfParagraph,
       ExecuteDeleteToBeginningOfParagraph, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteToEndOfLine, ExecuteDeleteToEndOfLine,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteToEndOfParagraph,
       ExecuteDeleteToEndOfParagraph, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteToMark, ExecuteDeleteToMark,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteWordBackward, ExecuteDeleteWordBackward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kDeleteWordForward, ExecuteDeleteWordForward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kFindString, ExecuteFindString, Supported,
       Enabled, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kFontName, StyleCommands::ExecuteFontName,
       Supported, EnabledInRichlyEditableText, StateNone,
       StyleCommands::ValueFontName, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kFontSize, StyleCommands::ExecuteFontSize,
       Supported, EnabledInRichlyEditableText, StateNone,
       StyleCommands::ValueFontSize, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kFontSizeDelta,
       StyleCommands::ExecuteFontSizeDelta, Supported,
       EnabledInRichlyEditableText, StateNone,
       StyleCommands::ValueFontSizeDelta, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kForeColor, StyleCommands::ExecuteForeColor,
       Supported, EnabledInRichlyEditableText, StateNone,
       StyleCommands::ValueForeColor, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kFormatBlock, ExecuteFormatBlock, Supported,
       EnabledInRichlyEditableText, StateNone, ValueFormatBlock,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kForwardDelete, ExecuteForwardDelete, Supported,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kHiliteColor, StyleCommands::ExecuteBackColor,
       Supported, EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kIgnoreSpelling, ExecuteIgnoreSpelling,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kIndent, ExecuteIndent, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertBacktab,
       InsertCommands::ExecuteInsertBacktab, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kIsTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertHTML, InsertCommands::ExecuteInsertHTML,
       Supported, EnabledInEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertHorizontalRule,
       InsertCommands::ExecuteInsertHorizontalRule, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertImage, InsertCommands::ExecuteInsertImage,
       Supported, EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertLineBreak,
       InsertCommands::ExecuteInsertLineBreak, Supported, EnabledInEditableText,
       StateNone, ValueStateOrNull, kIsTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertNewline,
       InsertCommands::ExecuteInsertNewline, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kIsTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertNewlineInQuotedContent,
       InsertCommands::ExecuteInsertNewlineInQuotedContent, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertOrderedList,
       InsertCommands::ExecuteInsertOrderedList, Supported,
       EnabledInRichlyEditableText, StateOrderedList, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertParagraph,
       InsertCommands::ExecuteInsertParagraph, Supported, EnabledInEditableText,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertTab, InsertCommands::ExecuteInsertTab,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kIsTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertText, InsertCommands::ExecuteInsertText,
       Supported, EnabledInEditableText, StateNone, ValueStateOrNull,
       kIsTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kInsertUnorderedList,
       InsertCommands::ExecuteInsertUnorderedList, Supported,
       EnabledInRichlyEditableText, StateUnorderedList, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kItalic, StyleCommands::ExecuteToggleItalic,
       Supported, EnabledInRichlyEditableText, StyleCommands::StateItalic,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kJustifyCenter, ExecuteJustifyCenter, Supported,
       EnabledInRichlyEditableText, StateJustifyCenter, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kJustifyFull, ExecuteJustifyFull, Supported,
       EnabledInRichlyEditableText, StateJustifyFull, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kJustifyLeft, ExecuteJustifyLeft, Supported,
       EnabledInRichlyEditableText, StateJustifyLeft, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kJustifyNone, ExecuteJustifyLeft, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kJustifyRight, ExecuteJustifyRight, Supported,
       EnabledInRichlyEditableText, StateJustifyRight, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMakeTextWritingDirectionLeftToRight,
       StyleCommands::ExecuteMakeTextWritingDirectionLeftToRight,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateTextWritingDirectionLeftToRight, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMakeTextWritingDirectionNatural,
       StyleCommands::ExecuteMakeTextWritingDirectionNatural,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateTextWritingDirectionNatural, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMakeTextWritingDirectionRightToLeft,
       StyleCommands::ExecuteMakeTextWritingDirectionRightToLeft,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateTextWritingDirectionRightToLeft, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveBackward, MoveCommands::ExecuteMoveBackward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveBackwardAndModifySelection,
       MoveCommands::ExecuteMoveBackwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveDown, MoveCommands::ExecuteMoveDown,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveDownAndModifySelection,
       MoveCommands::ExecuteMoveDownAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveForward, MoveCommands::ExecuteMoveForward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveForwardAndModifySelection,
       MoveCommands::ExecuteMoveForwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveLeft, MoveCommands::ExecuteMoveLeft,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveLeftAndModifySelection,
       MoveCommands::ExecuteMoveLeftAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMovePageDown, MoveCommands::ExecuteMovePageDown,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMovePageDownAndModifySelection,
       MoveCommands::ExecuteMovePageDownAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMovePageUp, MoveCommands::ExecuteMovePageUp,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMovePageUpAndModifySelection,
       MoveCommands::ExecuteMovePageUpAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveParagraphBackward,
       MoveCommands::ExecuteMoveParagraphBackward,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveParagraphBackwardAndModifySelection,
       MoveCommands::ExecuteMoveParagraphBackwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveParagraphForward,
       MoveCommands::ExecuteMoveParagraphForward, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveParagraphForwardAndModifySelection,
       MoveCommands::ExecuteMoveParagraphForwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveRight, MoveCommands::ExecuteMoveRight,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveRightAndModifySelection,
       MoveCommands::ExecuteMoveRightAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfDocument,
       MoveCommands::ExecuteMoveToBeginningOfDocument,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfDocumentAndModifySelection,
       MoveCommands::ExecuteMoveToBeginningOfDocumentAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfLine,
       MoveCommands::ExecuteMoveToBeginningOfLine,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfLineAndModifySelection,
       MoveCommands::ExecuteMoveToBeginningOfLineAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfParagraph,
       MoveCommands::ExecuteMoveToBeginningOfParagraph,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfParagraphAndModifySelection,
       MoveCommands::ExecuteMoveToBeginningOfParagraphAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfSentence,
       MoveCommands::ExecuteMoveToBeginningOfSentence,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToBeginningOfSentenceAndModifySelection,
       MoveCommands::ExecuteMoveToBeginningOfSentenceAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfDocument,
       MoveCommands::ExecuteMoveToEndOfDocument, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfDocumentAndModifySelection,
       MoveCommands::ExecuteMoveToEndOfDocumentAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfLine,
       MoveCommands::ExecuteMoveToEndOfLine, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfLineAndModifySelection,
       MoveCommands::ExecuteMoveToEndOfLineAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfParagraph,
       MoveCommands::ExecuteMoveToEndOfParagraph, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfParagraphAndModifySelection,
       MoveCommands::ExecuteMoveToEndOfParagraphAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfSentence,
       MoveCommands::ExecuteMoveToEndOfSentence, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToEndOfSentenceAndModifySelection,
       MoveCommands::ExecuteMoveToEndOfSentenceAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToLeftEndOfLine,
       MoveCommands::ExecuteMoveToLeftEndOfLine, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToLeftEndOfLineAndModifySelection,
       MoveCommands::ExecuteMoveToLeftEndOfLineAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToRightEndOfLine,
       MoveCommands::ExecuteMoveToRightEndOfLine, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveToRightEndOfLineAndModifySelection,
       MoveCommands::ExecuteMoveToRightEndOfLineAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveUp, MoveCommands::ExecuteMoveUp,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveUpAndModifySelection,
       MoveCommands::ExecuteMoveUpAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordBackward,
       MoveCommands::ExecuteMoveWordBackward, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordBackwardAndModifySelection,
       MoveCommands::ExecuteMoveWordBackwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordForward,
       MoveCommands::ExecuteMoveWordForward, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordForwardAndModifySelection,
       MoveCommands::ExecuteMoveWordForwardAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordLeft, MoveCommands::ExecuteMoveWordLeft,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordLeftAndModifySelection,
       MoveCommands::ExecuteMoveWordLeftAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordRight,
       MoveCommands::ExecuteMoveWordRight, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kMoveWordRightAndModifySelection,
       MoveCommands::ExecuteMoveWordRightAndModifySelection,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kOutdent, ExecuteOutdent, Supported,
       EnabledInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kOverWrite, ExecuteToggleOverwrite,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kPaste, ClipboardCommands::ExecutePaste,
       ClipboardCommands::PasteSupported, ClipboardCommands::EnabledPaste,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       ClipboardCommands::CanReadClipboard},
      {WebEditingCommandType::kPasteAndMatchStyle,
       ClipboardCommands::ExecutePasteAndMatchStyle, Supported,
       ClipboardCommands::EnabledPaste, StateNone, ValueStateOrNull,
       kNotTextInsertion, ClipboardCommands::CanReadClipboard},
      {WebEditingCommandType::kPasteGlobalSelection,
       ClipboardCommands::ExecutePasteGlobalSelection,
       SupportedFromMenuOrKeyBinding, ClipboardCommands::EnabledPaste,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       ClipboardCommands::CanReadClipboard},
      {WebEditingCommandType::kPrint, ExecutePrint, Supported, Enabled,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kRedo, ExecuteRedo, Supported, EnabledRedo,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kRemoveFormat, ExecuteRemoveFormat, Supported,
       EnabledRangeInEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollPageBackward, ExecuteScrollPageBackward,
       SupportedFromMenuOrKeyBinding, Enabled, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollPageForward, ExecuteScrollPageForward,
       SupportedFromMenuOrKeyBinding, Enabled, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollLineUp, ExecuteScrollLineUp,
       SupportedFromMenuOrKeyBinding, Enabled, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollLineDown, ExecuteScrollLineDown,
       SupportedFromMenuOrKeyBinding, Enabled, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollToBeginningOfDocument,
       ExecuteScrollToBeginningOfDocument, SupportedFromMenuOrKeyBinding,
       Enabled, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kScrollToEndOfDocument,
       ExecuteScrollToEndOfDocument, SupportedFromMenuOrKeyBinding, Enabled,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectAll, ExecuteSelectAll, Supported,
       EnabledSelectAll, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectLine, ExecuteSelectLine,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectParagraph, ExecuteSelectParagraph,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectSentence, ExecuteSelectSentence,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectToMark, ExecuteSelectToMark,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelectionAndMark, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSelectWord, ExecuteSelectWord,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSetMark, ExecuteSetMark,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelection, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kStrikethrough,
       StyleCommands::ExecuteStrikethrough, Supported,
       EnabledInRichlyEditableText, StyleCommands::StateStrikethrough,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kStyleWithCSS, StyleCommands::ExecuteStyleWithCSS,
       Supported, Enabled, StyleCommands::StateStyleWithCSS, ValueEmpty,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSubscript, StyleCommands::ExecuteSubscript,
       Supported, EnabledInRichlyEditableText, StyleCommands::StateSubscript,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSuperscript, StyleCommands::ExecuteSuperscript,
       Supported, EnabledInRichlyEditableText, StyleCommands::StateSuperscript,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kSwapWithMark, ExecuteSwapWithMark,
       SupportedFromMenuOrKeyBinding, EnabledVisibleSelectionAndMark, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kToggleBold, StyleCommands::ExecuteToggleBold,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateBold, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kToggleItalic, StyleCommands::ExecuteToggleItalic,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateItalic, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kToggleUnderline, StyleCommands::ExecuteUnderline,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText,
       StyleCommands::StateUnderline, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kTranspose, ExecuteTranspose, Supported,
       EnableCaretInEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUnderline, StyleCommands::ExecuteUnderline,
       Supported, EnabledInRichlyEditableText, StyleCommands::StateUnderline,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUndo, ExecuteUndo, Supported, EnabledUndo,
       StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUnlink, ExecuteUnlink, Supported,
       EnabledRangeInRichlyEditableText, StateNone, ValueStateOrNull,
       kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUnscript, StyleCommands::ExecuteUnscript,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUnselect, ExecuteUnselect, Supported,
       EnabledUnselect, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kUseCSS, StyleCommands::ExecuteUseCSS, Supported,
       Enabled, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kYank, ExecuteYank, SupportedFromMenuOrKeyBinding,
       EnabledInEditableText, StateNone, ValueStateOrNull, kNotTextInsertion,
       CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kYankAndSelect, ExecuteYankAndSelect,
       SupportedFromMenuOrKeyBinding, EnabledInEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
      {WebEditingCommandType::kAlignCenter, ExecuteJustifyCenter,
       SupportedFromMenuOrKeyBinding, EnabledInRichlyEditableText, StateNone,
       ValueStateOrNull, kNotTextInsertion, CanNotExecuteWhenDisabled},
  };
  // Handles all commands except WebEditingCommandType::Invalid.
  static_assert(
      arraysize(kEditorCommands) + 1 ==
          static_cast<size_t>(WebEditingCommandType::kNumberOfCommandTypes),
      "must handle all valid WebEditingCommandType");

  WebEditingCommandType command_type =
      WebEditingCommandTypeFromCommandName(command_name);
  if (command_type == WebEditingCommandType::kInvalid)
    return nullptr;

  int command_index = static_cast<int>(command_type) - 1;
  DCHECK(command_index >= 0 &&
         command_index < static_cast<int>(arraysize(kEditorCommands)));
  return &kEditorCommands[command_index];
}

EditorCommand Editor::CreateCommand(const String& command_name) const {
  return EditorCommand(InternalCommand(command_name),
                       EditorCommandSource::kMenuOrKeyBinding, frame_);
}

EditorCommand Editor::CreateCommand(const String& command_name,
                                    EditorCommandSource source) const {
  return EditorCommand(InternalCommand(command_name), source, frame_);
}

bool Editor::ExecuteCommand(const String& command_name) {
  // Specially handling commands that Editor::execCommand does not directly
  // support.
  DCHECK(GetFrame().GetDocument()->IsActive());
  if (command_name == "DeleteToEndOfParagraph") {
    if (!DeleteWithDirection(GetFrame(), DeleteDirection::kForward,
                             TextGranularity::kParagraphBoundary, true,
                             false)) {
      DeleteWithDirection(GetFrame(), DeleteDirection::kForward,
                          TextGranularity::kCharacter, true, false);
    }
    return true;
  }
  if (command_name == "DeleteBackward")
    return CreateCommand(AtomicString("BackwardDelete")).Execute();
  if (command_name == "DeleteForward")
    return CreateCommand(AtomicString("ForwardDelete")).Execute();
  if (command_name == "AdvanceToNextMisspelling") {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetFrame().GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

    // We need to pass false here or else the currently selected word will never
    // be skipped.
    GetSpellChecker().AdvanceToNextMisspelling(false);
    return true;
  }
  if (command_name == "ToggleSpellPanel") {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.
    // see http://crbug.com/590369 for more details.
    GetFrame().GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

    GetSpellChecker().ShowSpellingGuessPanel();
    return true;
  }
  return CreateCommand(command_name).Execute();
}

bool Editor::ExecuteCommand(const String& command_name, const String& value) {
  // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit
  // for editable nodes.
  DCHECK(GetFrame().GetDocument()->IsActive());
  if (!CanEdit() && command_name == "moveToBeginningOfDocument")
    return GetFrame().GetEventHandler().BubblingScroll(
        kScrollUpIgnoringWritingMode, kScrollByDocument);

  if (!CanEdit() && command_name == "moveToEndOfDocument")
    return GetFrame().GetEventHandler().BubblingScroll(
        kScrollDownIgnoringWritingMode, kScrollByDocument);

  if (command_name == "ToggleSpellPanel") {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited. see http://crbug.com/590369 for more details.
    GetFrame().GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();

    GetSpellChecker().ShowSpellingGuessPanel();
    return true;
  }

  return CreateCommand(command_name).Execute(value);
}

bool Editor::IsCommandEnabled(const String& command_name) const {
  return CreateCommand(command_name).IsEnabled();
}

EditorCommand::EditorCommand()
    : command_(nullptr), source_(EditorCommandSource::kMenuOrKeyBinding) {}

EditorCommand::EditorCommand(const EditorInternalCommand* command,
                             EditorCommandSource source,
                             LocalFrame* frame)
    : command_(command), source_(source), frame_(command ? frame : nullptr) {
  // Use separate assertions so we can tell which bad thing happened.
  if (!command)
    DCHECK(!frame_);
  else
    DCHECK(frame_);
}

LocalFrame& EditorCommand::GetFrame() const {
  DCHECK(frame_);
  return *frame_;
}

bool EditorCommand::Execute(const String& parameter,
                            Event* triggering_event) const {
  if (!CanExecute(triggering_event))
    return false;

  if (source_ == EditorCommandSource::kMenuOrKeyBinding) {
    InputEvent::InputType input_type =
        InputTypeFromCommandType(command_->command_type, *frame_);
    if (input_type != InputEvent::InputType::kNone) {
      if (DispatchBeforeInputEditorCommand(
              EventTargetNodeForDocument(frame_->GetDocument()), input_type,
              GetTargetRanges()) != DispatchEventResult::kNotCanceled)
        return true;
      // 'beforeinput' event handler may destroy target frame.
      if (frame_->GetDocument()->GetFrame() != frame_)
        return false;
    }
  }

  GetFrame().GetDocument()->UpdateStyleAndLayoutIgnorePendingStylesheets();
  DEFINE_STATIC_LOCAL(SparseHistogram, command_histogram,
                      ("WebCore.Editing.Commands"));
  command_histogram.Sample(static_cast<int>(command_->command_type));
  return command_->execute(*frame_, triggering_event, source_, parameter);
}

bool EditorCommand::Execute(Event* triggering_event) const {
  return Execute(String(), triggering_event);
}

bool EditorCommand::CanExecute(Event* triggering_event) const {
  if (IsEnabled(triggering_event))
    return true;
  return IsSupported() && frame_ && command_->can_execute(*frame_, source_);
}

bool EditorCommand::IsSupported() const {
  if (!command_)
    return false;
  switch (source_) {
    case EditorCommandSource::kMenuOrKeyBinding:
      return true;
    case EditorCommandSource::kDOM:
      return command_->is_supported_from_dom(frame_.Get());
  }
  NOTREACHED();
  return false;
}

bool EditorCommand::IsEnabled(Event* triggering_event) const {
  if (!IsSupported() || !frame_)
    return false;
  return command_->is_enabled(*frame_, triggering_event, source_);
}

EditingTriState EditorCommand::GetState(Event* triggering_event) const {
  if (!IsSupported() || !frame_)
    return EditingTriState::kFalse;
  return command_->state(*frame_, triggering_event);
}

String EditorCommand::Value(Event* triggering_event) const {
  if (!IsSupported() || !frame_)
    return String();
  return command_->value(*command_, *frame_, triggering_event);
}

bool EditorCommand::IsTextInsertion() const {
  return command_ && command_->is_text_insertion;
}

int EditorCommand::IdForHistogram() const {
  return IsSupported() ? static_cast<int>(command_->command_type) : 0;
}

const StaticRangeVector* EditorCommand::GetTargetRanges() const {
  const Node* target = EventTargetNodeForDocument(frame_->GetDocument());
  if (!IsSupported() || !frame_ || !target || !HasRichlyEditableStyle(*target))
    return nullptr;

  switch (command_->command_type) {
    case WebEditingCommandType::kDelete:
    case WebEditingCommandType::kDeleteBackward:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kBackward,
          TextGranularity::kCharacter);
    case WebEditingCommandType::kDeleteForward:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kForward,
          TextGranularity::kCharacter);
    case WebEditingCommandType::kDeleteToBeginningOfLine:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kBackward, TextGranularity::kLine);
    case WebEditingCommandType::kDeleteToBeginningOfParagraph:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kBackward,
          TextGranularity::kParagraph);
    case WebEditingCommandType::kDeleteToEndOfLine:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kForward, TextGranularity::kLine);
    case WebEditingCommandType::kDeleteToEndOfParagraph:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kForward,
          TextGranularity::kParagraph);
    case WebEditingCommandType::kDeleteWordBackward:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kBackward, TextGranularity::kWord);
    case WebEditingCommandType::kDeleteWordForward:
      return RangesFromCurrentSelectionOrExtendCaret(
          *frame_, SelectionModifyDirection::kForward, TextGranularity::kWord);
    default:
      return TargetRangesForInputEvent(*target);
  }
}

}  // namespace blink
