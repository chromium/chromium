// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/unicode_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {
namespace {

// Returns true if the search should ignore the given |node|'s contents. In
// other words, we don't need to recurse into the node's children.
bool ShouldIgnoreContents(const Node& node) {
  if (node.getNodeType() == Node::kCommentNode) {
    return true;
  }

  const auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return false;
  return (!element->ShouldSerializeEndTag() &&
          !IsA<HTMLInputElement>(*element)) ||
         (IsA<TextControlElement>(*element) &&
          !To<TextControlElement>(*element).SuggestedValue().empty()) ||
         IsA<HTMLIFrameElement>(*element) || IsA<HTMLImageElement>(*element) ||
         IsA<HTMLMeterElement>(*element) || IsA<HTMLObjectElement>(*element) ||
         IsA<HTMLProgressElement>(*element) ||
         (IsA<HTMLSelectElement>(*element) &&
          To<HTMLSelectElement>(*element).UsesMenuList()) ||
         IsA<HTMLStyleElement>(*element) || IsA<HTMLScriptElement>(*element) ||
         IsA<HTMLVideoElement>(*element) || IsA<HTMLAudioElement>(*element) ||
         (element->GetDisplayLockContext() &&
          element->GetDisplayLockContext()->IsLocked() &&
          !element->GetDisplayLockContext()->IsActivatable(
              DisplayLockActivationReason::kFindInPage));
}

// Returns the first ancestor element that isn't searchable. In other words,
// either ShouldIgnoreContents() returns true for it or it has a display: none
// style.  Returns nullptr if no such ancestor exists.
Node* GetOutermostNonSearchableAncestor(const Node& node) {
  Node* display_none = nullptr;
  for (Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    Element* element_ancestor = DynamicTo<Element>(&ancestor);
    if (!element_ancestor)
      continue;
    const ComputedStyle* style = element_ancestor->GetComputedStyle();
    if (!style || style->IsEnsuredInDisplayNone()) {
      display_none = element_ancestor;
      continue;
    }
    if (ShouldIgnoreContents(*element_ancestor))
      return element_ancestor;
    if (display_none)
      return display_none;
  }
  return nullptr;
}

const ComputedStyle* EnsureComputedStyleForFind(Node& node) {
  Element* element = DynamicTo<Element>(node);
  if (!element) {
    element = FlatTreeTraversal::ParentElement(node);
  }
  if (element) {
    return element->EnsureComputedStyle();
  }
  return nullptr;
}

// Returns the next/previous node after |start_node| (including start node) that
// is a text node and is searchable and visible.
template <class Direction>
Node* GetVisibleTextNode(Node& start_node) {
  Node* node = &start_node;
  // Move to outside display none subtree if we're inside one.
  while (Node* ancestor = GetOutermostNonSearchableAncestor(*node)) {
    if (!ancestor)
      return nullptr;
    node = Direction::NextSkippingSubtree(*ancestor);
    if (!node)
      return nullptr;
  }
  // Move to first text node that's visible.
  while (node) {
    const ComputedStyle* style = EnsureComputedStyleForFind(*node);
    if (ShouldIgnoreContents(*node) ||
        (style && style->Display() == EDisplay::kNone)) {
      // This element and its descendants are not visible, skip it.
      node = Direction::NextSkippingSubtree(*node);
      continue;
    }
    if (style && style->Visibility() == EVisibility::kVisible &&
        node->IsTextNode()) {
      return node;
    }
    // This element is hidden, but node might be visible,
    // or this is not a text node, so we move on.
    node = Direction::Next(*node);
  }
  return nullptr;
}

// Returns true if the given |node| is considered a 'block' for find-in-page,
// scroll-to-text and link-to-text even though it might not have a separate
// LayoutBlockFlow. For example, input fields should be considered a block
// boundary.
bool IsExplicitFindBoundary(const Node& node) {
  return IsTextControl(node);
}

// Checks if |start| appears before |end| in flat-tree order.
bool AreInOrder(const Node& start, const Node& end) {
  const Node* node = &start;
  while (node && !node->isSameNode(&end)) {
    node = FlatTreeTraversal::Next(*node);
  }
  return node->isSameNode(&end);
}

}  // namespace

// FindBuffer implementation.
FindBuffer::FindBuffer(const EphemeralRangeInFlatTree& range) {
  DCHECK(range.IsNotNull() && !range.IsCollapsed()) << range;
  CollectTextUntilBlockBoundary(range);
}

bool FindBuffer::IsInvalidMatch(MatchResultICU match) const {
  // Invalid matches are a result of accidentally matching elements that are
  // replaced with the max code point, and may lead to crashes. To avoid
  // crashing, we should skip the matches that are invalid - they would have
  // either an empty position or a non-offset-in-anchor position.
  const unsigned start_index = match.start;
  PositionInFlatTree start_position =
      PositionAtStartOfCharacterAtIndex(start_index);
  if (start_position.IsNull() || !start_position.IsOffsetInAnchor())
    return true;

  const unsigned end_index = match.start + match.length;
  DCHECK_LE(start_index, end_index);
  PositionInFlatTree end_position =
      PositionAtEndOfCharacterAtIndex(end_index - 1);
  if (end_position.IsNull() || !end_position.IsOffsetInAnchor())
    return true;
  return false;
}

EphemeralRangeInFlatTree FindBuffer::FindMatchInRange(
    const EphemeralRangeInFlatTree& range,
    String search_text,
    FindOptions options,
    absl::optional<base::TimeDelta> timeout_ms) {
  if (!range.StartPosition().IsConnected())
    return EphemeralRangeInFlatTree();

  base::TimeTicks start_time;

  EphemeralRangeInFlatTree last_match_range;
  Node* first_node = range.StartPosition().NodeAsRangeFirstNode();
  Node* past_last_node = range.EndPosition().NodeAsRangePastLastNode();
  Node* node = first_node;
  while (node && node != past_last_node) {
    if (start_time.is_null()) {
      start_time = base::TimeTicks::Now();
    } else {
      auto time_elapsed = base::TimeTicks::Now() - start_time;
      if (timeout_ms.has_value() && time_elapsed > timeout_ms.value()) {
        return EphemeralRangeInFlatTree(
            PositionInFlatTree::FirstPositionInNode(*node),
            PositionInFlatTree::FirstPositionInNode(*node));
      }
    }

    if (GetOutermostNonSearchableAncestor(*node)) {
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      continue;
    }
    if (!node->IsTextNode()) {
      node = FlatTreeTraversal::Next(*node);
      continue;
    }
    // If we're in the same node as the start position, start from the start
    // position instead of the start of this node.
    PositionInFlatTree start_position =
        node == first_node ? range.StartPosition()
                           : PositionInFlatTree::FirstPositionInNode(*node);
    if (start_position >= range.EndPosition())
      break;

    FindBuffer buffer(
        EphemeralRangeInFlatTree(start_position, range.EndPosition()));
    Results match_results = buffer.FindMatches(search_text, options);
    if (!match_results.IsEmpty()) {
      if (!(options & kBackwards)) {
        BufferMatchResult match = match_results.front();
        return buffer.RangeFromBufferIndex(match.start,
                                           match.start + match.length);
      }
      BufferMatchResult match = match_results.back();
      last_match_range =
          buffer.RangeFromBufferIndex(match.start, match.start + match.length);
    }
    node = buffer.PositionAfterBlock().ComputeContainerNode();
  }
  return last_match_range;
}

const Node& FindBuffer::GetFirstBlockLevelAncestorInclusive(const Node& node) {
  // Gets lowest inclusive ancestor that has block display value.
  // <div id=outer>a<div id=inner>b</div>c</div>
  // If we run this on "a" or "c" text node in we will get the outer div.
  // If we run it on the "b" text node we will get the inner div.
  if (!node.GetLayoutObject())
    return *node.GetDocument().documentElement();

  for (const Node& ancestor : FlatTreeTraversal::InclusiveAncestorsOf(node)) {
    if (!ancestor.GetLayoutObject())
      continue;
    if (!IsInSameUninterruptedBlock(ancestor, node))
      return ancestor;
  }

  return *node.GetDocument().documentElement();
}

bool FindBuffer::IsInSameUninterruptedBlock(const Node& start_node,
                                            const Node& end_node) {
  DCHECK(AreInOrder(start_node, end_node));
  DCHECK(start_node.GetLayoutObject());
  DCHECK(end_node.GetLayoutObject());

  if (start_node.isSameNode(&end_node))
    return true;

  if (IsExplicitFindBoundary(start_node) || IsExplicitFindBoundary(end_node))
    return false;

  LayoutBlockFlow& start_block_flow =
      *NGOffsetMapping::GetInlineFormattingContextOf(
          *start_node.GetLayoutObject());
  LayoutBlockFlow& end_block_flow =
      *NGOffsetMapping::GetInlineFormattingContextOf(
          *end_node.GetLayoutObject());
  if (start_block_flow != end_block_flow)
    return false;

  // It's possible that 2 nodes are in the same block flow but there is a node
  // in between that has a separate block flow. An example is an input field.
  for (const Node* node = &start_node; !node->isSameNode(&end_node);
       node = FlatTreeTraversal::Next(*node)) {
    const ComputedStyle* style = node->GetComputedStyle();
    if (ShouldIgnoreContents(*node) || !style ||
        style->Display() == EDisplay::kNone ||
        style->Visibility() != EVisibility::kVisible)
      continue;

    if (node->GetLayoutObject() &&
        *NGOffsetMapping::GetInlineFormattingContextOf(
            *node->GetLayoutObject()) != start_block_flow)
      return false;
  }

  return true;
}

Node* FindBuffer::ForwardVisibleTextNode(Node& start_node) {
  struct ForwardDirection {
    static Node* Next(const Node& node) {
      return FlatTreeTraversal::Next(node);
    }
    static Node* NextSkippingSubtree(const Node& node) {
      return FlatTreeTraversal::NextSkippingChildren(node);
    }
  };
  return GetVisibleTextNode<ForwardDirection>(start_node);
}

Node* FindBuffer::BackwardVisibleTextNode(Node& start_node) {
  struct BackwardDirection {
    static Node* Next(const Node& node) {
      return FlatTreeTraversal::Previous(node);
    }
    static Node* NextSkippingSubtree(const Node& node) {
      // Unlike |NextSkippingChildren|, |Previous| already skips given nodes
      // subtree.
      return FlatTreeTraversal::Previous(node);
    }
  };
  return GetVisibleTextNode<BackwardDirection>(start_node);
}

FindBuffer::Results FindBuffer::FindMatches(const WebString& search_text,
                                            const blink::FindOptions options) {
  // We should return empty result if it's impossible to get a match (buffer is
  // empty or too short), or when something went wrong in layout, in which case
  // |offset_mapping_| is null.
  if (buffer_.empty() || search_text.length() > buffer_.size() ||
      !offset_mapping_)
    return Results();
  String search_text_16_bit = search_text;
  search_text_16_bit.Ensure16Bit();
  FoldQuoteMarksAndSoftHyphens(search_text_16_bit);
  return Results(*this, &text_searcher_, buffer_, search_text_16_bit, options);
}

void FindBuffer::CollectTextUntilBlockBoundary(
    const EphemeralRangeInFlatTree& range) {
  // Collects text until block boundary located at or after |start_node|
  // to |buffer_|. Saves the next starting node after the block to
  // |node_after_block_|.
  DCHECK(range.IsNotNull() && !range.IsCollapsed()) << range;

  node_after_block_ = nullptr;
  const Node* const first_node = range.StartPosition().NodeAsRangeFirstNode();
  if (!first_node)
    return;
  // Get first visible text node from |start_position|.
  Node* node =
      ForwardVisibleTextNode(*range.StartPosition().NodeAsRangeFirstNode());
  if (!node || !node->isConnected())
    return;

  const Node& block_ancestor = GetFirstBlockLevelAncestorInclusive(*node);
  const Node* just_after_block = FlatTreeTraversal::Next(
      FlatTreeTraversal::LastWithinOrSelf(block_ancestor));

  // Collect all text under |block_ancestor| to |buffer_|,
  // unless we meet another block on the way. If so, we should split.
  // Example: <div id="outer">a<span>b</span>c<div>d</div></div>
  // Will try to collect all text in outer div but will actually
  // stop when it encounters the inner div. So buffer will be "abc".

  // Used for checking if we reached a new block.
  Node* last_added_text_node = nullptr;

  // We will also stop if we encountered/passed |end_node|.
  Node* end_node = range.EndPosition().NodeAsRangeLastNode();

  while (node && node != just_after_block) {
    if (ShouldIgnoreContents(*node)) {
      if (end_node && (end_node == node ||
                       FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
        // For setting |node_after_block| later.
        node = FlatTreeTraversal::NextSkippingChildren(*node);
        break;
      }
      // Replace the node with char constants so we wouldn't encounter this node
      // or its descendants later.
      ReplaceNodeWithCharConstants(*node);
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      continue;
    }
    const ComputedStyle* style = EnsureComputedStyleForFind(*node);
    if (style->Display() == EDisplay::kNone) {
      // This element and its descendants are not visible, skip it.
      // We can safely just check the computed style of this node since
      // we guarantee |block_ancestor| is visible.
      if (end_node && (end_node == node ||
                       FlatTreeTraversal::IsDescendantOf(*end_node, *node))) {
        // For setting |node_after_block| later.
        node = FlatTreeTraversal::NextSkippingChildren(*node);
        break;
      }
      node = FlatTreeTraversal::NextSkippingChildren(*node);
      if (node && !FlatTreeTraversal::IsDescendantOf(*node, block_ancestor))
        break;
      continue;
    }

    if (style->Visibility() == EVisibility::kVisible &&
        node->GetLayoutObject()) {
      // This node is in its own sub-block separate from our starting position.
      if (last_added_text_node && last_added_text_node->GetLayoutObject() &&
          !IsInSameUninterruptedBlock(*last_added_text_node, *node))
        break;

      const auto* text_node = DynamicTo<Text>(node);
      if (text_node) {
        last_added_text_node = node;
        LayoutBlockFlow& block_flow =
            *NGOffsetMapping::GetInlineFormattingContextOf(
                *text_node->GetLayoutObject());
        AddTextToBuffer(*text_node, block_flow, range);
      }
    }
    if (node == end_node) {
      node = FlatTreeTraversal::Next(*node);
      break;
    }
    node = FlatTreeTraversal::Next(*node);
  }
  node_after_block_ = node;
  FoldQuoteMarksAndSoftHyphens(buffer_.data(), buffer_.size());
}

void FindBuffer::ReplaceNodeWithCharConstants(const Node& node) {
  if (!IsA<HTMLElement>(node))
    return;

  if (IsA<HTMLWBRElement>(To<HTMLElement>(node)))
    return;

  if (IsA<HTMLBRElement>(To<HTMLElement>(node))) {
    buffer_.push_back(kNewlineCharacter);
    return;
  }

  buffer_.push_back(kMaxCodepoint);
}

EphemeralRangeInFlatTree FindBuffer::RangeFromBufferIndex(
    unsigned start_index,
    unsigned end_index) const {
  DCHECK_LE(start_index, end_index);
  PositionInFlatTree start_position =
      PositionAtStartOfCharacterAtIndex(start_index);
  PositionInFlatTree end_position =
      PositionAtEndOfCharacterAtIndex(end_index - 1);
  return EphemeralRangeInFlatTree(start_position, end_position);
}

const FindBuffer::BufferNodeMapping* FindBuffer::MappingForIndex(
    unsigned index) const {
  // Get the first entry that starts at a position higher than offset, and
  // move back one entry.
  auto* it = std::upper_bound(
      buffer_node_mappings_.begin(), buffer_node_mappings_.end(), index,
      [](const unsigned offset, const BufferNodeMapping& entry) {
        return offset < entry.offset_in_buffer;
      });
  if (it == buffer_node_mappings_.begin())
    return nullptr;
  auto* entry = std::prev(it);
  return entry;
}

PositionInFlatTree FindBuffer::PositionAtStartOfCharacterAtIndex(
    unsigned index) const {
  DCHECK_LT(index, buffer_.size());
  DCHECK(offset_mapping_);
  const BufferNodeMapping* entry = MappingForIndex(index);
  if (!entry)
    return PositionInFlatTree();
  return ToPositionInFlatTree(offset_mapping_->GetLastPosition(
      index - entry->offset_in_buffer + entry->offset_in_mapping));
}

PositionInFlatTree FindBuffer::PositionAtEndOfCharacterAtIndex(
    unsigned index) const {
  DCHECK_LT(index, buffer_.size());
  DCHECK(offset_mapping_);
  const BufferNodeMapping* entry = MappingForIndex(index);
  if (!entry)
    return PositionInFlatTree();
  return ToPositionInFlatTree(offset_mapping_->GetFirstPosition(
      index - entry->offset_in_buffer + entry->offset_in_mapping + 1));
}

void FindBuffer::AddTextToBuffer(const Text& text_node,
                                 LayoutBlockFlow& block_flow,
                                 const EphemeralRangeInFlatTree& range) {
  if (!offset_mapping_) {
    offset_mapping_ = NGInlineNode::GetOffsetMapping(&block_flow);

    if (UNLIKELY(!offset_mapping_)) {
      // TODO(crbug.com/955678): There are certain cases where we fail to
      // compute the |NGOffsetMapping| due to failures in layout. As the root
      // cause is hard to fix at the moment, we just work around it here.
      return;
    }
  }

  Position node_start =
      (&text_node == range.StartPosition().ComputeContainerNode())
          ? ToPositionInDOMTree(range.StartPosition().ToOffsetInAnchor())
          : Position::FirstPositionInNode(text_node);
  Position node_end =
      (&text_node == range.EndPosition().ComputeContainerNode())
          ? ToPositionInDOMTree(range.EndPosition().ToOffsetInAnchor())
          : Position::LastPositionInNode(text_node);
  unsigned last_unit_end = 0;
  bool first_unit = true;
  const String mapped_text = offset_mapping_->GetText();
  for (const NGOffsetMappingUnit& unit :
       offset_mapping_->GetMappingUnitsForDOMRange(
           EphemeralRange(node_start, node_end))) {
    if (first_unit || last_unit_end != unit.TextContentStart()) {
      // This is the first unit, or the units are not consecutive, so we need to
      // insert a new BufferNodeMapping.
      buffer_node_mappings_.push_back(
          BufferNodeMapping({buffer_.size(), unit.TextContentStart()}));
      first_unit = false;
    }
    String text_for_unit =
        mapped_text.Substring(unit.TextContentStart(),
                              unit.TextContentEnd() - unit.TextContentStart());
    text_for_unit.Ensure16Bit();
    buffer_.Append(text_for_unit.Characters16(), text_for_unit.length());
    last_unit_end = unit.TextContentEnd();
  }
}

// FindBuffer::Results implementation.
FindBuffer::Results::Results() {
  empty_result_ = true;
}

FindBuffer::Results::Results(const FindBuffer& find_buffer,
                             TextSearcherICU* text_searcher,
                             const Vector<UChar>& buffer,
                             const String& search_text,
                             const blink::FindOptions options) {
  // We need to own the |search_text| because |text_searcher_| only has a
  // StringView (doesn't own the search text).
  search_text_ = search_text;
  find_buffer_ = &find_buffer;
  text_searcher_ = text_searcher;
  text_searcher_->SetPattern(search_text_, options);
  text_searcher_->SetText(buffer.data(), buffer.size());
  text_searcher_->SetOffset(0);
}

FindBuffer::Results::Iterator FindBuffer::Results::begin() const {
  if (empty_result_)
    return end();
  text_searcher_->SetOffset(0);
  return Iterator(*find_buffer_, text_searcher_);
}

FindBuffer::Results::Iterator FindBuffer::Results::end() const {
  return Iterator();
}

bool FindBuffer::Results::IsEmpty() const {
  return begin() == end();
}

FindBuffer::BufferMatchResult FindBuffer::Results::front() const {
  return *begin();
}

FindBuffer::BufferMatchResult FindBuffer::Results::back() const {
  Iterator last_result;
  for (Iterator it = begin(); it != end(); ++it) {
    last_result = it;
  }
  return *last_result;
}

unsigned FindBuffer::Results::CountForTesting() const {
  unsigned result = 0;
  for (Iterator it = begin(); it != end(); ++it) {
    ++result;
  }
  return result;
}

// Findbuffer::Results::Iterator implementation.
FindBuffer::Results::Iterator::Iterator(const FindBuffer& find_buffer,
                                        TextSearcherICU* text_searcher)
    : find_buffer_(&find_buffer),
      text_searcher_(text_searcher),
      has_match_(true) {
  operator++();
}

const FindBuffer::BufferMatchResult FindBuffer::Results::Iterator::operator*()
    const {
  DCHECK(has_match_);
  return FindBuffer::BufferMatchResult({match_.start, match_.length});
}

void FindBuffer::Results::Iterator::operator++() {
  DCHECK(has_match_);
  has_match_ = text_searcher_->NextMatchResult(match_);
  if (has_match_ && find_buffer_ && find_buffer_->IsInvalidMatch(match_))
    operator++();
}

}  // namespace blink
