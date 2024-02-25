// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_incremental_text_command.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_code_point_state_machine.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"

namespace blink {

namespace {

wtf_size_t ComputeCommonPrefixLength(const String& str1, const String& str2) {
  const wtf_size_t max_common_prefix_length =
      std::min(str1.length(), str2.length());
  ForwardCodePointStateMachine code_point_state_machine;
  wtf_size_t result = 0;
  for (wtf_size_t index = 0; index < max_common_prefix_length; ++index) {
    if (str1[index] != str2[index])
      return result;
    code_point_state_machine.FeedFollowingCodeUnit(str1[index]);
    if (!code_point_state_machine.AtCodePointBoundary())
      continue;
    result = index;
  }
  return max_common_prefix_length;
}

wtf_size_t ComputeCommonSuffixLength(const String& str1, const String& str2) {
  const wtf_size_t length1 = str1.length();
  const wtf_size_t length2 = str2.length();
  const wtf_size_t max_common_suffix_length = std::min(length1, length2);
  for (wtf_size_t index = 0; index < max_common_suffix_length; ++index) {
    if (str1[length1 - index - 1] != str2[length2 - index - 1])
      return index;
  }
  return max_common_suffix_length;
}

wtf_size_t ComputeCommonGraphemeClusterPrefixLength(
    const Position& selection_start,
    const String& old_text,
    const String& new_text) {
  const wtf_size_t common_prefix_length =
      ComputeCommonPrefixLength(old_text, new_text);
  const int selection_offset = selection_start.ComputeOffsetInContainerNode();
  const ContainerNode* selection_node =
      selection_start.ComputeContainerNode()->parentNode();

  // Calculate offset from |selection_node| start to |selection_start|'s
  // container node start.
  CharacterIterator forward_iterator(
      EphemeralRange::RangeOfContents(*selection_node),
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  const Position selection_start_container_node_start(
      selection_start.ComputeContainerNode(), 0);
  int offset = 0;
  while (!forward_iterator.AtEnd()) {
    const Position& current_position = forward_iterator.StartPosition();
    if (current_position == selection_start_container_node_start)
      break;
    forward_iterator.Advance(1);
    offset++;
  }

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(offset, offset + selection_offset + common_prefix_length)
          .CreateRange(*selection_node);
  if (range.IsNull())
    return 0;
  const Position& position = range.EndPosition();
  const wtf_size_t diff = ComputeDistanceToLeftGraphemeBoundary(position);
  DCHECK_GE(common_prefix_length, diff);
  return common_prefix_length - diff;
}

wtf_size_t ComputeCommonGraphemeClusterSuffixLength(
    const Position& selection_start,
    const String& old_text,
    const String& new_text) {
  const wtf_size_t common_suffix_length =
      ComputeCommonSuffixLength(old_text, new_text);
  const int selection_offset = selection_start.ComputeOffsetInContainerNode();
  const ContainerNode* selection_node =
      selection_start.ComputeContainerNode()->parentNode();

  // For grapheme cluster, we should adjust it for grapheme boundary.
  const EphemeralRange& range =
      PlainTextRange(
          0, selection_offset + old_text.length() - common_suffix_length)
          .CreateRange(*selection_node);
  if (range.IsNull())
    return 0;
  const Position& position = range.EndPosition();
  const wtf_size_t diff = ComputeDistanceToRightGraphemeBoundary(position);
  if (diff > common_suffix_length)
    return 0;
  return common_suffix_length - diff;
}

const String ComputeTextForInsertion(const String& new_text,
                                     const wtf_size_t common_prefix_length,
                                     const wtf_size_t common_suffix_length) {
  return new_text.Substring(
      common_prefix_length,
      new_text.length() - common_prefix_length - common_suffix_length);
}

SelectionInDOMTree ComputeSelectionForInsertion(
    const EphemeralRange& selection_range,
    const int offset,
    const int length) {
  CharacterIterator char_it(
      selection_range,
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  const EphemeralRange& range_for_insertion =
      char_it.CalculateCharacterSubrange(offset, length);
  return SelectionInDOMTree::Builder()
      .SetBaseAndExtent(range_for_insertion)
      .Build();
}

}  // anonymous namespace

InsertIncrementalTextCommand::InsertIncrementalTextCommand(
    Document& document,
    const String& text,
    RebalanceType rebalance_type)
    : InsertTextCommand(document, text, rebalance_type) {}

void InsertIncrementalTextCommand::DoApply(EditingState* editing_state) {
  DCHECK(!GetDocument().NeedsLayoutTreeUpdate());
  const Element* element = RootEditableElementOf(EndingSelection().Anchor());
  DCHECK(element);

  const VisibleSelection& visible_selection = EndingVisibleSelection();
  const EphemeralRange selection_range(visible_selection.Start(),
                                       visible_selection.End());
  const String old_text = PlainText(
      selection_range,
      TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
  const String& new_text = text_;

  const Position& selection_start = visible_selection.Start();
  const wtf_size_t new_text_length = new_text.length();
  const wtf_size_t old_text_length = old_text.length();
  const wtf_size_t common_prefix_length =
      ComputeCommonGraphemeClusterPrefixLength(selection_start, old_text,
                                               new_text);
  // We should ignore common prefix when finding common suffix.
  const wtf_size_t common_suffix_length =
      ComputeCommonGraphemeClusterSuffixLength(
          selection_start,
          old_text.Right(old_text_length - common_prefix_length),
          new_text.Right(new_text_length - common_prefix_length));
  DCHECK_GE(old_text_length, common_prefix_length + common_suffix_length);

  text_ = ComputeTextForInsertion(text_, common_prefix_length,
                                  common_suffix_length);

  const int offset = static_cast<int>(common_prefix_length);
  const int length = static_cast<int>(old_text_length - common_prefix_length -
                                      common_suffix_length);
  const VisibleSelection& selection_for_insertion = CreateVisibleSelection(
      ComputeSelectionForInsertion(selection_range, offset, length));

  SetEndingSelectionWithoutValidation(selection_for_insertion.Start(),
                                      selection_for_insertion.End());

  InsertTextCommand::DoApply(editing_state);
}

}  // namespace blink
