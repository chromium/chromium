// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_controller.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/markers/spell_check_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_replacement_scope.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/editing/suggestion/text_suggestion_info.h"
#include "third_party/blink/renderer/core/frame/frame_view.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

namespace {

bool ShouldDeleteNextCharacter(const Node& marker_text_node,
                               const DocumentMarker& marker) {
  // If the character immediately following the range to be deleted is a space,
  // delete it if either of these conditions holds:
  // - We're deleting at the beginning of the editable text (to avoid ending up
  //   with a space at the beginning)
  // - The character immediately before the range being deleted is also a space
  //   (to avoid ending up with two adjacent spaces)
  const EphemeralRange next_character_range =
      PlainTextRange(marker.EndOffset(), marker.EndOffset() + 1)
          .CreateRange(*marker_text_node.parentNode());
  // No character immediately following the range (so it can't be a space)
  if (next_character_range.IsNull())
    return false;

  const String next_character_str =
      PlainText(next_character_range, TextIteratorBehavior::Builder().Build());
  const UChar next_character = next_character_str[0];
  // Character immediately following the range is not a space
  if (next_character != kSpaceCharacter &&
      next_character != kNoBreakSpaceCharacter)
    return false;

  // First case: we're deleting at the beginning of the editable text
  if (marker.StartOffset() == 0)
    return true;

  const EphemeralRange prev_character_range =
      PlainTextRange(marker.StartOffset() - 1, marker.StartOffset())
          .CreateRange(*marker_text_node.parentNode());
  // Not at beginning, but there's no character immediately before the range
  // being deleted (so it can't be a space)
  if (prev_character_range.IsNull())
    return false;

  const String prev_character_str =
      PlainText(prev_character_range, TextIteratorBehavior::Builder().Build());
  // Return true if the character immediately before the range is a space, false
  // otherwise
  const UChar prev_character = prev_character_str[0];
  return prev_character == kSpaceCharacter ||
         prev_character == kNoBreakSpaceCharacter;
}

EphemeralRangeInFlatTree ComputeRangeSurroundingCaret(
    const PositionInFlatTree& caret_position) {
  const unsigned position_offset_in_node =
      caret_position.ComputeOffsetInContainerNode();
  auto* text_node = DynamicTo<Text>(caret_position.ComputeContainerNode());
  // If we're in the interior of a text node, we can avoid calling
  // PreviousPositionOf/NextPositionOf for better efficiency.
  if (text_node && position_offset_in_node != 0 &&
      position_offset_in_node != text_node->length()) {
    return EphemeralRangeInFlatTree(
        PositionInFlatTree(text_node, position_offset_in_node - 1),
        PositionInFlatTree(text_node, position_offset_in_node + 1));
  }

  const PositionInFlatTree& previous_position =
      PreviousPositionOf(caret_position, PositionMoveType::kGraphemeCluster);

  const PositionInFlatTree& next_position =
      NextPositionOf(caret_position, PositionMoveType::kGraphemeCluster);

  return EphemeralRangeInFlatTree(
      previous_position.IsNull() ? caret_position : previous_position,
      next_position.IsNull() ? caret_position : next_position);
}

struct SuggestionInfosWithNodeAndHighlightColor {
  STACK_ALLOCATED();

 public:
  Persistent<const Text> text_node;
  Color highlight_color;
  Vector<TextSuggestionInfo> suggestion_infos;
};

SuggestionInfosWithNodeAndHighlightColor ComputeSuggestionInfos(
    const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
        node_suggestion_marker_pairs,
    size_t max_number_of_suggestions) {
  // We look at all suggestion markers touching or overlapping the touched
  // location to pull suggestions from. We preferentially draw suggestions from
  // shorter markers first (since we assume they're more specific to the tapped
  // location) until we hit our limit.
  HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
      node_suggestion_marker_pairs_sorted_by_length =
          node_suggestion_marker_pairs;
  std::sort(node_suggestion_marker_pairs_sorted_by_length.begin(),
            node_suggestion_marker_pairs_sorted_by_length.end(),
            [](const std::pair<const Text*, DocumentMarker*>& pair1,
               const std::pair<const Text*, DocumentMarker*>& pair2) {
              const int length1 =
                  pair1.second->EndOffset() - pair1.second->StartOffset();
              const int length2 =
                  pair2.second->EndOffset() - pair2.second->StartOffset();
              return length1 < length2;
            });

  SuggestionInfosWithNodeAndHighlightColor
      suggestion_infos_with_node_and_highlight_color;
  // In theory, a user could tap right before/after the start of a node and we'd
  // want to pull in suggestions from either side of the tap. However, this is
  // an edge case that's unlikely to matter in practice (the user will most
  // likely just tap in the node where they want to apply the suggestions) and
  // it complicates implementation, so we require that all suggestions come
  // from the same text node.
  suggestion_infos_with_node_and_highlight_color.text_node =
      node_suggestion_marker_pairs_sorted_by_length.front().first;

  // The highlight color comes from the shortest suggestion marker touching or
  // intersecting the tapped location. If there's no color set, we use the
  // default text selection color.
  const auto* first_suggestion_marker = To<SuggestionMarker>(
      node_suggestion_marker_pairs_sorted_by_length.front().second.Get());

  suggestion_infos_with_node_and_highlight_color.highlight_color =
      (first_suggestion_marker->SuggestionHighlightColor() ==
       Color::kTransparent)
          ? LayoutTheme::TapHighlightColor()
          : first_suggestion_marker->SuggestionHighlightColor();

  Vector<TextSuggestionInfo>& suggestion_infos =
      suggestion_infos_with_node_and_highlight_color.suggestion_infos;
  for (const std::pair<Member<const Text>, Member<DocumentMarker>>&
           node_marker_pair : node_suggestion_marker_pairs_sorted_by_length) {
    if (node_marker_pair.first !=
        suggestion_infos_with_node_and_highlight_color.text_node)
      continue;

    if (suggestion_infos.size() == max_number_of_suggestions)
      break;

    const auto* marker = To<SuggestionMarker>(node_marker_pair.second.Get());
    const Vector<String>& marker_suggestions = marker->Suggestions();
    for (wtf_size_t suggestion_index = 0;
         suggestion_index < marker_suggestions.size(); ++suggestion_index) {
      const String& suggestion = marker_suggestions[suggestion_index];
      if (suggestion_infos.size() == max_number_of_suggestions)
        break;
      if (base::ranges::any_of(
              suggestion_infos,
              [marker, &suggestion](const TextSuggestionInfo& info) {
                return info.span_start == (int32_t)marker->StartOffset() &&
                       info.span_end == (int32_t)marker->EndOffset() &&
                       info.suggestion == suggestion;
              })) {
        continue;
      }

      TextSuggestionInfo suggestion_info;
      suggestion_info.marker_tag = marker->Tag();
      suggestion_info.suggestion_index = suggestion_index;
      suggestion_info.span_start = marker->StartOffset();
      suggestion_info.span_end = marker->EndOffset();
      suggestion_info.suggestion = suggestion;
      suggestion_infos.push_back(suggestion_info);
    }
  }

  return suggestion_infos_with_node_and_highlight_color;
}

}  // namespace

TextSuggestionController::TextSuggestionController(LocalDOMWindow& window)
    : is_suggestion_menu_open_(false),
      window_(&window),
      text_suggestion_host_(&window) {}

bool TextSuggestionController::IsMenuOpen() const {
  return is_suggestion_menu_open_;
}

void TextSuggestionController::HandlePotentialSuggestionTap(
    const PositionInFlatTree& caret_position) {
  if (!IsAvailable() || GetFrame() != GetDocument().GetFrame()) {
    // TODO(crbug.com/1054955, crbug.com/1409155, crbug.com/1412036): Callsites
    // should not call this function in these conditions.
    return;
  }

  // It's theoretically possible, but extremely unlikely, that the user has
  // managed to tap on some text after TextSuggestionController has told the
  // browser to open the text suggestions menu, but before the browser has
  // actually done so. In this case, we should just ignore the tap.
  if (is_suggestion_menu_open_)
    return;

  const EphemeralRangeInFlatTree& range_to_check =
      ComputeRangeSurroundingCaret(caret_position);

  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerIntersectingRange(
          range_to_check,
          DocumentMarker::MarkerTypes(DocumentMarker::kSpelling |
                                      DocumentMarker::kGrammar |
                                      DocumentMarker::kSuggestion));
  if (!node_and_marker.first)
    return;

  const auto* marker = DynamicTo<SuggestionMarker>(node_and_marker.second);
  if (marker && marker->Suggestions().empty())
    return;

  if (!text_suggestion_host_.is_bound()) {
    GetFrame().GetBrowserInterfaceBroker().GetInterface(
        text_suggestion_host_.BindNewPipeAndPassReceiver(
            GetFrame().GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  text_suggestion_host_->StartSuggestionMenuTimer();
}

void TextSuggestionController::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(text_suggestion_host_);
}

void TextSuggestionController::ReplaceActiveSuggestionRange(
    const String& suggestion) {
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone())
    return;

  const EphemeralRangeInFlatTree& range_to_check =
      selection.IsRange() ? selection.ToNormalizedEphemeralRange()
                          : ComputeRangeSurroundingCaret(selection.Start());
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_marker_pairs =
          GetFrame().GetDocument()->Markers().MarkersIntersectingRange(
              range_to_check, DocumentMarker::MarkerTypes::ActiveSuggestion());

  if (node_marker_pairs.empty())
    return;

  const Text* const marker_text_node = node_marker_pairs.front().first;
  const DocumentMarker* const marker = node_marker_pairs.front().second;

  const EphemeralRange& range_to_replace =
      EphemeralRange(Position(marker_text_node, marker->StartOffset()),
                     Position(marker_text_node, marker->EndOffset()));
  ReplaceRangeWithText(range_to_replace, suggestion);
}

void TextSuggestionController::ApplySpellCheckSuggestion(
    const String& suggestion) {
  ReplaceActiveSuggestionRange(suggestion);
  OnSuggestionMenuClosed();
}

void TextSuggestionController::ApplyTextSuggestion(int32_t marker_tag,
                                                   uint32_t suggestion_index) {
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone()) {
    OnSuggestionMenuClosed();
    return;
  }

  const EphemeralRangeInFlatTree& range_to_check =
      selection.IsRange() ? selection.ToNormalizedEphemeralRange()
                          : ComputeRangeSurroundingCaret(selection.Start());

  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_marker_pairs =
          GetFrame().GetDocument()->Markers().MarkersIntersectingRange(
              range_to_check, DocumentMarker::MarkerTypes::Suggestion());

  const Text* marker_text_node = nullptr;
  SuggestionMarker* marker = nullptr;
  for (const std::pair<Member<const Text>, Member<DocumentMarker>>&
           node_marker_pair : node_marker_pairs) {
    auto* suggestion_marker =
        To<SuggestionMarker>(node_marker_pair.second.Get());
    if (suggestion_marker->Tag() == marker_tag) {
      marker_text_node = node_marker_pair.first;
      marker = suggestion_marker;
      break;
    }
  }

  if (!marker) {
    OnSuggestionMenuClosed();
    return;
  }
  DCHECK(marker_text_node);
  const EphemeralRange& range_to_replace =
      EphemeralRange(Position(marker_text_node, marker->StartOffset()),
                     Position(marker_text_node, marker->EndOffset()));

  const String& replacement = marker->Suggestions()[suggestion_index];
  const String& new_suggestion = PlainText(range_to_replace);

  {
    SuggestionMarkerReplacementScope scope;
    ReplaceRangeWithText(range_to_replace, replacement);
  }

  if (marker->IsMisspelling()) {
    GetFrame().GetDocument()->Markers().RemoveSuggestionMarkerByTag(
        *marker_text_node, marker->Tag());
  } else {
    marker->SetSuggestion(suggestion_index, new_suggestion);
  }

  OnSuggestionMenuClosed();
}

void TextSuggestionController::DeleteActiveSuggestionRange() {
  AttemptToDeleteActiveSuggestionRange();
  OnSuggestionMenuClosed();
}

void TextSuggestionController::OnNewWordAddedToDictionary(const String& word) {
  // Android pops up a dialog to let the user confirm they actually want to add
  // the word to the dictionary; this method gets called as soon as the dialog
  // is shown. So the word isn't actually in the dictionary here, even if the
  // user will end up confirming the dialog, and we shouldn't try to re-run
  // spellcheck here.

  // Note: this actually matches the behavior in native Android text boxes
  GetDocument().Markers().RemoveSpellingMarkersUnderWords(
      Vector<String>({word}));
  OnSuggestionMenuClosed();
}

void TextSuggestionController::OnSuggestionMenuClosed() {
  if (!IsAvailable())
    return;

  GetDocument().Markers().RemoveMarkersOfTypes(
      DocumentMarker::MarkerTypes::ActiveSuggestion());
  GetFrame().Selection().SetCaretEnabled(true);
  is_suggestion_menu_open_ = false;
}

void TextSuggestionController::SuggestionMenuTimeoutCallback(
    size_t max_number_of_suggestions) {
  if (!IsAvailable())
    return;

  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone())
    return;

  const EphemeralRangeInFlatTree& range_to_check =
      selection.IsRange() ? selection.ToNormalizedEphemeralRange()
                          : ComputeRangeSurroundingCaret(selection.Start());

  // We can show a menu if the user tapped on either a spellcheck marker or a
  // suggestion marker. Suggestion markers take precedence (we don't even try
  // to draw both underlines, suggestion wins).
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
      node_suggestion_marker_pairs =
          GetFrame().GetDocument()->Markers().MarkersIntersectingRange(
              range_to_check, DocumentMarker::MarkerTypes::Suggestion());
  if (!node_suggestion_marker_pairs.empty()) {
    ShowSuggestionMenu(node_suggestion_marker_pairs, max_number_of_suggestions);
    return;
  }

  // If we didn't find any suggestion markers, look for spell check markers.
  const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>
      node_spelling_marker_pairs =
          GetFrame().GetDocument()->Markers().MarkersIntersectingRange(
              range_to_check, DocumentMarker::MarkerTypes::Misspelling());
  if (!node_spelling_marker_pairs.empty())
    ShowSpellCheckMenu(node_spelling_marker_pairs.front());

  // If we get here, that means the user tapped on a spellcheck or suggestion
  // marker a few hundred milliseconds ago (to start the double-click timer)
  // but it's gone now. Oh well...
}

void TextSuggestionController::ShowSpellCheckMenu(
    const std::pair<const Text*, DocumentMarker*>& node_spelling_marker_pair) {
  const Text* const marker_text_node = node_spelling_marker_pair.first;
  auto* const marker = To<SpellCheckMarker>(node_spelling_marker_pair.second);

  const EphemeralRange active_suggestion_range =
      EphemeralRange(Position(marker_text_node, marker->StartOffset()),
                     Position(marker_text_node, marker->EndOffset()));
  const String& misspelled_word = PlainText(active_suggestion_range);
  const String& description = marker->Description();

  is_suggestion_menu_open_ = true;
  GetFrame().Selection().SetCaretEnabled(false);
  GetDocument().Markers().AddActiveSuggestionMarker(
      active_suggestion_range, Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kNone,
      ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent,
      LayoutTheme::GetTheme().PlatformActiveSpellingMarkerHighlightColor());

  Vector<String> suggestions;
  description.Split('\n', suggestions);

  Vector<mojom::blink::SpellCheckSuggestionPtr> suggestion_ptrs;
  for (const String& suggestion : suggestions) {
    mojom::blink::SpellCheckSuggestionPtr info_ptr(
        mojom::blink::SpellCheckSuggestion::New());
    info_ptr->suggestion = suggestion;
    suggestion_ptrs.push_back(std::move(info_ptr));
  }

  // |FrameSelection::AbsoluteCaretBounds()| requires clean layout.
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  GetFrame().GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kSpellCheck);
  const gfx::Rect& absolute_bounds =
      GetFrame().Selection().AbsoluteCaretBounds();
  const gfx::Rect& viewport_bounds =
      GetFrame().View()->FrameToViewport(absolute_bounds);

  text_suggestion_host_->ShowSpellCheckSuggestionMenu(
      viewport_bounds.x(), viewport_bounds.bottom(), std::move(misspelled_word),
      std::move(suggestion_ptrs));
}

void TextSuggestionController::ShowSuggestionMenu(
    const HeapVector<std::pair<Member<const Text>, Member<DocumentMarker>>>&
        node_suggestion_marker_pairs,
    size_t max_number_of_suggestions) {
  DCHECK(!node_suggestion_marker_pairs.empty());

  SuggestionInfosWithNodeAndHighlightColor
      suggestion_infos_with_node_and_highlight_color = ComputeSuggestionInfos(
          node_suggestion_marker_pairs, max_number_of_suggestions);

  Vector<TextSuggestionInfo>& suggestion_infos =
      suggestion_infos_with_node_and_highlight_color.suggestion_infos;
  if (suggestion_infos.empty())
    return;

  int span_union_start = suggestion_infos[0].span_start;
  int span_union_end = suggestion_infos[0].span_end;
  for (wtf_size_t i = 1; i < suggestion_infos.size(); ++i) {
    span_union_start =
        std::min(span_union_start, suggestion_infos[i].span_start);
    span_union_end = std::max(span_union_end, suggestion_infos[i].span_end);
  }

  const Text* text_node =
      suggestion_infos_with_node_and_highlight_color.text_node;
  for (TextSuggestionInfo& info : suggestion_infos) {
    const EphemeralRange prefix_range(Position(text_node, span_union_start),
                                      Position(text_node, info.span_start));
    const String& prefix = PlainText(prefix_range);

    const EphemeralRange suffix_range(Position(text_node, info.span_end),
                                      Position(text_node, span_union_end));
    const String& suffix = PlainText(suffix_range);

    info.prefix = prefix;
    info.suffix = suffix;
  }

  const EphemeralRange marker_range(Position(text_node, span_union_start),
                                    Position(text_node, span_union_end));

  GetDocument().Markers().AddActiveSuggestionMarker(
      marker_range, Color::kTransparent, ui::mojom::ImeTextSpanThickness::kThin,
      ui::mojom::ImeTextSpanUnderlineStyle::kSolid, Color::kTransparent,
      suggestion_infos_with_node_and_highlight_color.highlight_color);

  is_suggestion_menu_open_ = true;
  GetFrame().Selection().SetCaretEnabled(false);

  const String& misspelled_word = PlainText(marker_range);
  CallMojoShowTextSuggestionMenu(
      suggestion_infos_with_node_and_highlight_color.suggestion_infos,
      misspelled_word);
}

void TextSuggestionController::CallMojoShowTextSuggestionMenu(
    const Vector<TextSuggestionInfo>& text_suggestion_infos,
    const String& misspelled_word) {
  Vector<mojom::blink::TextSuggestionPtr> suggestion_info_ptrs;
  for (const blink::TextSuggestionInfo& info : text_suggestion_infos) {
    mojom::blink::TextSuggestionPtr info_ptr(
        mojom::blink::TextSuggestion::New());
    info_ptr->marker_tag = info.marker_tag;
    info_ptr->suggestion_index = info.suggestion_index;
    info_ptr->prefix = info.prefix;
    info_ptr->suggestion = info.suggestion;
    info_ptr->suffix = info.suffix;

    suggestion_info_ptrs.push_back(std::move(info_ptr));
  }

  const gfx::Rect& absolute_bounds =
      GetFrame().Selection().AbsoluteCaretBounds();
  const gfx::Rect& viewport_bounds =
      GetFrame().View()->FrameToViewport(absolute_bounds);

  text_suggestion_host_->ShowTextSuggestionMenu(
      viewport_bounds.x(), viewport_bounds.bottom(), misspelled_word,
      std::move(suggestion_info_ptrs));
}

Document& TextSuggestionController::GetDocument() const {
  DCHECK(IsAvailable());
  return *window_->document();
}

bool TextSuggestionController::IsAvailable() const {
  return !window_->IsContextDestroyed();
}

LocalFrame& TextSuggestionController::GetFrame() const {
  DCHECK(window_->GetFrame());
  return *window_->GetFrame();
}

std::pair<const Node*, const DocumentMarker*>
TextSuggestionController::FirstMarkerIntersectingRange(
    const EphemeralRangeInFlatTree& range,
    DocumentMarker::MarkerTypes types) const {
  const Node* const range_start_container =
      range.StartPosition().ComputeContainerNode();
  const unsigned range_start_offset =
      range.StartPosition().ComputeOffsetInContainerNode();
  const Node* const range_end_container =
      range.EndPosition().ComputeContainerNode();
  const unsigned range_end_offset =
      range.EndPosition().ComputeOffsetInContainerNode();

  for (const Node& node : range.Nodes()) {
    auto* text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;

    const unsigned start_offset =
        node == range_start_container ? range_start_offset : 0;
    const unsigned end_offset =
        node == range_end_container ? range_end_offset : text_node->length();

    const DocumentMarker* const found_marker =
        GetFrame().GetDocument()->Markers().FirstMarkerIntersectingOffsetRange(
            *text_node, start_offset, end_offset, types);
    if (found_marker)
      return std::make_pair(&node, found_marker);
  }

  return {};
}

std::pair<const Node*, const DocumentMarker*>
TextSuggestionController::FirstMarkerTouchingSelection(
    DocumentMarker::MarkerTypes types) const {
  const VisibleSelectionInFlatTree& selection =
      GetFrame().Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone())
    return {};

  const EphemeralRangeInFlatTree& range_to_check =
      selection.IsRange()
          ? EphemeralRangeInFlatTree(selection.Start(), selection.End())
          : ComputeRangeSurroundingCaret(selection.Start());

  return FirstMarkerIntersectingRange(range_to_check, types);
}

void TextSuggestionController::AttemptToDeleteActiveSuggestionRange() {
  const std::pair<const Node*, const DocumentMarker*>& node_and_marker =
      FirstMarkerTouchingSelection(
          DocumentMarker::MarkerTypes::ActiveSuggestion());
  if (!node_and_marker.first)
    return;

  const Node* const marker_text_node = node_and_marker.first;
  const DocumentMarker* const marker = node_and_marker.second;

  const bool delete_next_char =
      ShouldDeleteNextCharacter(*marker_text_node, *marker);

  const EphemeralRange range_to_delete = EphemeralRange(
      Position(marker_text_node, marker->StartOffset()),
      Position(marker_text_node, marker->EndOffset() + delete_next_char));
  ReplaceRangeWithText(range_to_delete, "");
}

void TextSuggestionController::ReplaceRangeWithText(const EphemeralRange& range,
                                                    const String& replacement) {
  GetFrame().Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().SetBaseAndExtent(range).Build());

  InsertTextAndSendInputEventsOfTypeInsertReplacementText(GetFrame(),
                                                          replacement);
}

}  // namespace blink
