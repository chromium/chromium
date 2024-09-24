/*
 * Copyright (C) 2006, 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"

#include "third_party/blink/public/platform/web_spell_check_panel_host_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/public/web/web_text_decoration_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/cold_mode_spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_requester.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Returns whether ranges [0, checking_range_length) and
// [location, location + length) intersect
bool CheckingRangeCovers(int checking_range_length, int location, int length) {
  DCHECK_GE(checking_range_length, 0);
  DCHECK_GE(length, 0);
  return location + length > 0 && location < checking_range_length;
}

bool IsWhiteSpaceOrPunctuation(UChar c) {
  return IsSpaceOrNewline(c) || WTF::unicode::IsPunct(c);
}

}  // namespace

static WebSpellCheckPanelHostClient& GetEmptySpellCheckPanelHostClient() {
  DEFINE_STATIC_LOCAL(EmptySpellCheckPanelHostClient, client, ());
  return client;
}

WebSpellCheckPanelHostClient& SpellChecker::SpellCheckPanelHostClient() const {
  WebSpellCheckPanelHostClient* spell_check_panel_host_client =
      GetFrame().Client()->SpellCheckPanelHostClient();
  if (!spell_check_panel_host_client)
    return GetEmptySpellCheckPanelHostClient();
  return *spell_check_panel_host_client;
}

WebTextCheckClient* SpellChecker::GetTextCheckerClient() const {
  // There is no frame client if the frame is detached.
  if (!GetFrame().Client())
    return nullptr;
  return GetFrame().Client()->GetTextCheckerClient();
}

SpellChecker::SpellChecker(LocalDOMWindow& window)
    : window_(&window),
      spell_check_requester_(MakeGarbageCollected<SpellCheckRequester>(window)),
      idle_spell_check_controller_(
          MakeGarbageCollected<IdleSpellCheckController>(
              window,
              *spell_check_requester_)) {}

LocalFrame& SpellChecker::GetFrame() const {
  DCHECK(window_->GetFrame());
  return *window_->GetFrame();
}

bool SpellChecker::IsSpellCheckingEnabled() const {
  if (WebTextCheckClient* client = GetTextCheckerClient())
    return client->IsSpellCheckingEnabled();
  return false;
}

void SpellChecker::IgnoreSpelling() {
  RemoveMarkers(GetFrame()
                    .Selection()
                    .ComputeVisibleSelectionInDOMTree()
                    .ToNormalizedEphemeralRange(),
                DocumentMarker::MarkerTypes::Spelling());
}

void SpellChecker::AdvanceToNextMisspelling(bool start_before_selection) {
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame().GetDocument()->Lifecycle());

  // The basic approach is to search in two phases - from the selection end to
  // the end of the doc, and then we wrap and search from the doc start to
  // (approximately) where we started.

  // Start at the end of the selection, search to edge of document. Starting at
  // the selection end makes repeated "check spelling" commands work.
  VisibleSelection selection(
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree());
  Position spelling_search_start, spelling_search_end;
  Range::selectNodeContents(GetFrame().GetDocument(), spelling_search_start,
                            spelling_search_end);

  bool started_with_selection = false;
  if (selection.Start().AnchorNode()) {
    started_with_selection = true;
    if (start_before_selection) {
      VisiblePosition start(selection.VisibleStart());
      // We match AppKit's rule: Start 1 character before the selection.
      VisiblePosition one_before_start = PreviousPositionOf(start);
      spelling_search_start =
          (one_before_start.IsNotNull() ? one_before_start : start)
              .ToParentAnchoredPosition();
    } else {
      spelling_search_start = selection.VisibleEnd().ToParentAnchoredPosition();
    }
  }

  Position position = spelling_search_start;
  if (!IsEditablePosition(position)) {
    // This shouldn't happen in very often because the Spelling menu items
    // aren't enabled unless the selection is editable.  This can happen in Mail
    // for a mix of non-editable and editable content (like Stationary), when
    // spell checking the whole document before sending the message.  In that
    // case the document might not be editable, but there are editable pockets
    // that need to be spell checked.

    if (!GetFrame().GetDocument()->documentElement())
      return;
    position = CreateVisiblePosition(
                   FirstEditablePositionAfterPositionInRoot(
                       position, *GetFrame().GetDocument()->documentElement()))
                   .DeepEquivalent();
    if (position.IsNull())
      return;

    spelling_search_start = position.ParentAnchoredEquivalent();
    started_with_selection = false;  // won't need to wrap
  }

  // topNode defines the whole range we want to operate on
  ContainerNode* top_node = HighestEditableRoot(position);
  // TODO(yosin): |lastOffsetForEditing()| is wrong here if
  // |editingIgnoresContent(highestEditableRoot())| returns true, e.g. <table>
  spelling_search_end = Position::EditingPositionOf(
      top_node, EditingStrategy::LastOffsetForEditing(top_node));

  // If spellingSearchRange starts in the middle of a word, advance to the
  // next word so we start checking at a word boundary. Going back by one char
  // and then forward by a word does the trick.
  if (started_with_selection) {
    const Position& one_before_start =
        PreviousPositionOf(CreateVisiblePosition(spelling_search_start))
            .DeepEquivalent();
    if (one_before_start.IsNotNull() &&
        RootEditableElementOf(one_before_start) ==
            RootEditableElementOf(spelling_search_start)) {
      spelling_search_start =
          CreateVisiblePosition(EndOfWordPosition(one_before_start),
                                TextAffinity::kUpstreamIfPossible)
              .ToParentAnchoredPosition();
    }
    // else we were already at the start of the editable node
  }

  if (spelling_search_start == spelling_search_end)
    return;  // nothing to search in

  // We go to the end of our first range instead of the start of it, just to be
  // sure we don't get foiled by any word boundary problems at the start. It
  // means we might do a tiny bit more searching.
  Node* search_end_node_after_wrap = spelling_search_end.ComputeContainerNode();
  int search_end_offset_after_wrap =
      spelling_search_end.OffsetInContainerNode();

  std::pair<String, int> misspelled_item(String(), 0);
  String& misspelled_word = misspelled_item.first;
  int& misspelling_offset = misspelled_item.second;
  misspelled_item =
      FindFirstMisspelling(spelling_search_start, spelling_search_end);

  // If we did not find a misspelled word, wrap and try again (but don't bother
  // if we started at the beginning of the block rather than at a selection).
  if (started_with_selection && !misspelled_word) {
    spelling_search_start = Position::EditingPositionOf(top_node, 0);
    // going until the end of the very first chunk we tested is far enough
    spelling_search_end = Position::EditingPositionOf(
        search_end_node_after_wrap, search_end_offset_after_wrap);
    misspelled_item =
        FindFirstMisspelling(spelling_search_start, spelling_search_end);
  }

  if (misspelled_word.empty()) {
    SpellCheckPanelHostClient().UpdateSpellingUIWithMisspelledWord({});
  } else {
    // We found a misspelling. Select the misspelling, update the spelling
    // panel, and store a marker so we draw the red squiggle later.

    const EphemeralRange misspelling_range = CalculateCharacterSubrange(
        EphemeralRange(spelling_search_start, spelling_search_end),
        misspelling_offset, misspelled_word.length());
    GetFrame().Selection().SetSelectionAndEndTyping(
        SelectionInDOMTree::Builder()
            .SetBaseAndExtent(misspelling_range)
            .Build());
    GetFrame().Selection().RevealSelection();
    SpellCheckPanelHostClient().UpdateSpellingUIWithMisspelledWord(
        misspelled_word);
    GetFrame().GetDocument()->Markers().AddSpellingMarker(misspelling_range);
  }
}

void SpellChecker::ShowSpellingGuessPanel() {
  if (SpellCheckPanelHostClient().IsShowingSpellingUI()) {
    SpellCheckPanelHostClient().ShowSpellingUI(false);
    return;
  }

  AdvanceToNextMisspelling(true);
  SpellCheckPanelHostClient().ShowSpellingUI(true);
}

static void AddMarker(Document* document,
                      const EphemeralRange& checking_range,
                      DocumentMarker::MarkerType type,
                      int location,
                      int length,
                      const Vector<String>& descriptions) {
  DCHECK(type == DocumentMarker::kSpelling || type == DocumentMarker::kGrammar)
      << type;
  DCHECK_GT(length, 0);
  DCHECK_GE(location, 0);
  const EphemeralRange& range_to_mark =
      CalculateCharacterSubrange(checking_range, location, length);
  if (!SpellChecker::IsSpellCheckingEnabledAt(range_to_mark.StartPosition()))
    return;
  if (!SpellChecker::IsSpellCheckingEnabledAt(range_to_mark.EndPosition()))
    return;

  StringBuilder description;
  for (wtf_size_t i = 0; i < descriptions.size(); ++i) {
    if (i != 0)
      description.Append('\n');
    description.Append(descriptions[i]);
  }

  if (type == DocumentMarker::kSpelling) {
    document->Markers().AddSpellingMarker(range_to_mark,
                                          description.ToString());
    return;
  }

  DCHECK_EQ(type, DocumentMarker::kGrammar);
  document->Markers().AddGrammarMarker(range_to_mark, description.ToString());
}

void SpellChecker::MarkAndReplaceFor(
    SpellCheckRequest* request,
    const Vector<TextCheckingResult>& results) {
  TRACE_EVENT0("blink", "SpellChecker::markAndReplaceFor");
  DCHECK(request);
  if (!GetFrame().Selection().IsAvailable()) {
    // "editing/spelling/spellcheck-async-remove-frame.html" reaches here.
    return;
  }
  if (!request->IsValid())
    return;
  if (request->RootEditableElement()->GetDocument() !=
      GetFrame().Selection().GetDocument()) {
    // we ignore |request| made for another document.
    // "editing/spelling/spellcheck-sequencenum.html" and others reach here.
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame().GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSpellCheck);

  EphemeralRange checking_range(request->CheckingRange());

  // Abort marking if the content of the checking change has been modified.
  String current_content =
      PlainText(checking_range, TextIteratorBehavior::Builder()
                                    .SetEmitsObjectReplacementCharacter(true)
                                    .Build());
  if (current_content != request->GetText()) {
    // "editing/spelling/spellcheck-async-mutation.html" reaches here.
    return;
  }

  // Clear the stale markers.
  RemoveMarkers(checking_range, DocumentMarker::MarkerTypes::Misspelling());

  if (!results.size())
    return;

  const int checking_range_length = TextIterator::RangeLength(checking_range);
  for (const TextCheckingResult& result : results) {
    const int result_location = result.location;
    const int result_length = result.length;

    // Only mark misspelling if result falls within checking range.
    switch (result.decoration) {
      case kTextDecorationTypeSpelling:
        if (result_location < 0 ||
            result_location + result_length > checking_range_length)
          continue;
        AddMarker(GetFrame().GetDocument(), checking_range,
                  DocumentMarker::kSpelling, result_location, result_length,
                  result.replacements);
        continue;

      case kTextDecorationTypeGrammar:
        if (!CheckingRangeCovers(checking_range_length, result_location,
                                 result_length)) {
          continue;
        }
        DCHECK_GT(result_length, 0);
        DCHECK_GE(result_location, 0);
        for (const GrammarDetail& detail : result.details) {
          DCHECK_GT(detail.length, 0);
          DCHECK_GE(detail.location, 0);
          if (!CheckingRangeCovers(checking_range_length,
                                   result_location + detail.location,
                                   detail.length)) {
            continue;
          }
          AddMarker(GetFrame().GetDocument(), checking_range,
                    DocumentMarker::kGrammar, result_location + detail.location,
                    detail.length, result.replacements);
        }
        continue;
    }
    NOTREACHED_IN_MIGRATION();
  }
}

void SpellChecker::DidEndEditingOnTextField(Element* e) {
  TRACE_EVENT0("blink", "SpellChecker::didEndEditingOnTextField");

  // Remove markers when deactivating a selection in an <input type="text"/>.
  // Prevent new ones from appearing too.
  HTMLElement* inner_editor = ToTextControl(e)->InnerEditorElement();
  RemoveSpellingAndGrammarMarkers(*inner_editor);
}

void SpellChecker::RemoveSpellingAndGrammarMarkers(const HTMLElement& element,
                                                   ElementsType elements_type) {
  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  See http://crbug.com/590369 for more details.
  if (elements_type == ElementsType::kOnlyNonEditable) {
    GetFrame().GetDocument()->UpdateStyleAndLayoutTreeForElement(
        &element, DocumentUpdateReason::kSpellCheck);
  }

  for (Node& node : NodeTraversal::InclusiveDescendantsOf(element)) {
    auto* text_node = DynamicTo<Text>(node);
    if ((elements_type == ElementsType::kAll || !IsEditable(node)) &&
        text_node) {
      GetFrame().GetDocument()->Markers().RemoveMarkersForNode(
          *text_node, DocumentMarker::MarkerTypes::Misspelling());
    }
  }
}

DocumentMarkerGroup* SpellChecker::GetSpellCheckMarkerGroupUnderSelection()
    const {
  const VisibleSelection& selection =
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsNone())
    return {};

  // Caret and range selections always return valid normalized ranges.
  const EphemeralRange& selection_range = FirstEphemeralRangeOf(selection);

  return GetFrame()
      .GetDocument()
      ->Markers()
      .FirstMarkerGroupIntersectingEphemeralRange(
          selection_range, DocumentMarker::MarkerTypes::Misspelling());
}

std::pair<String, String> SpellChecker::SelectMisspellingAsync() {
  const DocumentMarkerGroup* const marker_group =
      GetSpellCheckMarkerGroupUnderSelection();
  if (!marker_group)
    return {};

  const VisibleSelection& selection =
      GetFrame().Selection().ComputeVisibleSelectionInDOMTree();
  // Caret and range selections (one of which we must have since we found a
  // marker) always return valid normalized ranges.
  const EphemeralRange& selection_range =
      selection.ToNormalizedEphemeralRange();

  const EphemeralRange marker_range(marker_group->StartPosition(),
                                    marker_group->EndPosition());
  const String& marked_text = PlainText(marker_range);
  if (marked_text.StripWhiteSpace(&IsWhiteSpaceOrPunctuation) !=
      PlainText(selection_range).StripWhiteSpace(&IsWhiteSpaceOrPunctuation))
    return {};
  const Text* text_node =
      To<Text>(selection_range.StartPosition().ComputeContainerNode());
  const SpellCheckMarker* marker =
      To<SpellCheckMarker>(marker_group->GetMarkerForText(text_node));
  return std::make_pair(marked_text, marker->Description());
}

void SpellChecker::ReplaceMisspelledRange(const String& text) {
  const DocumentMarkerGroup* const marker_group =
      GetSpellCheckMarkerGroupUnderSelection();
  if (!marker_group)
    return;

  GetFrame().Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(marker_group->StartPosition())
          .Extend(marker_group->EndPosition())
          .Build());

  InsertTextAndSendInputEventsOfTypeInsertReplacementText(GetFrame(), text);
}

void SpellChecker::RespondToChangedSelection() {
  idle_spell_check_controller_->RespondToChangedSelection();
}

void SpellChecker::RespondToChangedContents() {
  idle_spell_check_controller_->RespondToChangedContents();
}

void SpellChecker::RespondToChangedEnablement(const HTMLElement& element,
                                              bool enabled) {
  if (enabled) {
    idle_spell_check_controller_->RespondToChangedEnablement();
  } else {
    RemoveSpellingAndGrammarMarkers(element);
    idle_spell_check_controller_->SetSpellCheckingDisabled(element);
  }
}

void SpellChecker::RemoveSpellingMarkers() {
  GetFrame().GetDocument()->Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::Misspelling());
}

void SpellChecker::RemoveSpellingMarkersUnderWords(
    const Vector<String>& words) {
  DocumentMarkerController& marker_controller =
      GetFrame().GetDocument()->Markers();
  marker_controller.RemoveSpellingMarkersUnderWords(words);
}

static Node* FindFirstMarkable(Node* node) {
  while (node) {
    LayoutObject* layout_object = node->GetLayoutObject();
    if (!layout_object)
      return nullptr;
    if (layout_object->IsText())
      return node;
    if (layout_object->IsTextControl()) {
      node = To<TextControlElement>(node)
                 ->VisiblePositionForIndex(1)
                 .DeepEquivalent()
                 .AnchorNode();
    } else if (node->hasChildren()) {
      node = node->firstChild();
    } else {
      node = node->nextSibling();
    }
  }

  return nullptr;
}

bool SpellChecker::SelectionStartHasMarkerFor(
    DocumentMarker::MarkerType marker_type,
    int from,
    int length) const {
  Node* node = FindFirstMarkable(GetFrame()
                                     .Selection()
                                     .ComputeVisibleSelectionInDOMTree()
                                     .Start()
                                     .AnchorNode());
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return false;

  unsigned start_offset = static_cast<unsigned>(from);
  unsigned end_offset = static_cast<unsigned>(from + length);
  DocumentMarkerVector markers =
      GetFrame().GetDocument()->Markers().MarkersFor(*text_node);
  for (wtf_size_t i = 0; i < markers.size(); ++i) {
    DocumentMarker* marker = markers[i];
    if (marker->StartOffset() <= start_offset &&
        end_offset <= marker->EndOffset() && marker->GetType() == marker_type)
      return true;
  }

  return false;
}

void SpellChecker::RemoveMarkers(const EphemeralRange& range,
                                 DocumentMarker::MarkerTypes marker_types) {
  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());

  if (range.IsNull())
    return;

  GetFrame().GetDocument()->Markers().RemoveMarkersInRange(range, marker_types);
}

void SpellChecker::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(spell_check_requester_);
  visitor->Trace(idle_spell_check_controller_);
}

Vector<TextCheckingResult> SpellChecker::FindMisspellings(const String& text) {
  Vector<UChar> characters;
  text.AppendTo(characters);

  TextBreakIterator* iterator = WordBreakIterator(characters);
  if (!iterator)
    return Vector<TextCheckingResult>();

  Vector<TextCheckingResult> results;
  int word_start = iterator->current();
  while (word_start >= 0) {
    int word_end = iterator->next();
    if (word_end < 0)
      break;
    size_t word_length = word_end - word_start;
    size_t misspelling_location = 0;
    size_t misspelling_length = 0;
    if (WebTextCheckClient* text_checker_client = GetTextCheckerClient()) {
      // SpellCheckWord will write (0, 0) into the output vars, which is what
      // our caller expects if the word is spelled correctly.
      text_checker_client->CheckSpelling(
          String(characters.data() + word_start, word_length),
          misspelling_location, misspelling_length, nullptr);
    } else {
      misspelling_location = 0;
    }
    if (misspelling_length > 0) {
      DCHECK_GE(misspelling_location, 0u);
      DCHECK_LE(misspelling_location + misspelling_length, word_length);
      TextCheckingResult misspelling;
      misspelling.decoration = kTextDecorationTypeSpelling;
      misspelling.location =
          base::checked_cast<int>(word_start + misspelling_location);
      misspelling.length = base::checked_cast<int>(misspelling_length);
      results.push_back(misspelling);
    }
    word_start = word_end;
  }
  return results;
}

std::pair<String, int> SpellChecker::FindFirstMisspelling(const Position& start,
                                                          const Position& end) {
  String misspelled_word;

  // Initialize out parameters; they will be updated if we find something to
  // return.
  String first_found_item;
  int first_found_offset = 0;

  // Expand the search range to encompass entire paragraphs, since text checking
  // needs that much context. Determine the character offset from the start of
  // the paragraph to the start of the original search range, since we will want
  // to ignore results in this area.
  EphemeralRange paragraph_range =
      ExpandToParagraphBoundary(EphemeralRange(start, start));
  Position paragraph_start = paragraph_range.StartPosition();
  Position paragraph_end = paragraph_range.EndPosition();

  const int total_range_length =
      TextIterator::RangeLength(paragraph_start, end);
  const int range_start_offset =
      TextIterator::RangeLength(paragraph_start, start);
  int total_length_processed = 0;

  bool first_iteration = true;
  bool last_iteration = false;
  while (total_length_processed < total_range_length) {
    // Iterate through the search range by paragraphs, checking each one for
    // spelling.
    int current_length =
        TextIterator::RangeLength(paragraph_start, paragraph_end);
    int current_start_offset = first_iteration ? range_start_offset : 0;
    int current_end_offset = current_length;
    if (InSameParagraph(CreateVisiblePosition(paragraph_start),
                        CreateVisiblePosition(end))) {
      // Determine the character offset from the end of the original search
      // range to the end of the paragraph, since we will want to ignore results
      // in this area.
      current_end_offset = TextIterator::RangeLength(paragraph_start, end);
      last_iteration = true;
    }
    if (current_start_offset < current_end_offset) {
      String paragraph_string = PlainText(paragraph_range);
      if (paragraph_string.length() > 0) {
        int spelling_location = 0;

        Vector<TextCheckingResult> results = FindMisspellings(paragraph_string);

        for (unsigned i = 0; i < results.size(); i++) {
          const TextCheckingResult* result = &results[i];
          if (result->location >= current_start_offset &&
              result->location + result->length <= current_end_offset) {
            DCHECK_GT(result->length, 0);
            DCHECK_GE(result->location, 0);
            spelling_location = result->location;
            misspelled_word =
                paragraph_string.Substring(result->location, result->length);
            DCHECK(misspelled_word.length());
            break;
          }
        }

        if (!misspelled_word.empty()) {
          int spelling_offset = spelling_location - current_start_offset;
          if (!first_iteration)
            spelling_offset +=
                TextIterator::RangeLength(start, paragraph_start);
          first_found_offset = spelling_offset;
          first_found_item = misspelled_word;
          break;
        }
      }
    }
    if (last_iteration ||
        total_length_processed + current_length >= total_range_length)
      break;
    Position new_paragraph_start =
        StartOfNextParagraph(CreateVisiblePosition(paragraph_end))
            .DeepEquivalent();
    if (new_paragraph_start.IsNull())
      break;

    paragraph_range = ExpandToParagraphBoundary(
        EphemeralRange(new_paragraph_start, new_paragraph_start));
    paragraph_start = paragraph_range.StartPosition();
    paragraph_end = paragraph_range.EndPosition();
    first_iteration = false;
    total_length_processed += current_length;
  }
  return std::make_pair(first_found_item, first_found_offset);
}

void SpellChecker::ElementRemoved(Element* element) {
  GetIdleSpellCheckController().GetColdModeRequester().ElementRemoved(element);
}

// static
bool SpellChecker::IsSpellCheckingEnabledAt(const Position& position) {
  if (position.IsNull())
    return false;
  if (TextControlElement* text_control = EnclosingTextControl(position)) {
    if (auto* input = DynamicTo<HTMLInputElement>(text_control)) {
      if (!input->IsFocusedElementInDocument())
        return false;
    }
  }
  HTMLElement* element =
      Traversal<HTMLElement>::FirstAncestorOrSelf(*position.AnchorNode());
  return element && element->IsSpellCheckingEnabled() && IsEditable(*element);
}

STATIC_ASSERT_ENUM(kWebTextDecorationTypeSpelling, kTextDecorationTypeSpelling);
STATIC_ASSERT_ENUM(kWebTextDecorationTypeGrammar, kTextDecorationTypeGrammar);

}  // namespace blink
