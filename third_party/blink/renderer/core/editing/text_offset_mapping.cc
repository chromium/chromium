// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/text_offset_mapping.h"

#include <ostream>
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"

namespace blink {

namespace {

// TODO(editing-dev): We may not need to do full-subtree traversal, but we're
// not sure, e.g. ::first-line. See |enum PseudoId| for list of pseudo elements
// used in Blink.
bool HasNonPsuedoNode(const LayoutObject& parent) {
  if (parent.NonPseudoNode())
    return true;
  for (const LayoutObject* runner = &parent; runner;
       runner = runner->NextInPreOrder(&parent)) {
    if (runner->NonPseudoNode())
      return true;
  }
  // Following HTML reach here:
  //  [1] <div style="columns: 5 31px">...</div>; http://crbug.com/832055
  //  [2] <select></select>; http://crbug.com/834623
  return false;
}

bool CanBeInlineContentsContainer(const LayoutObject& layout_object) {
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(layout_object);
  if (!block_flow)
    return false;
  if (!block_flow->ChildrenInline() || block_flow->IsAtomicInlineLevel())
    return false;
  if (block_flow->IsRuby()) {
    // We should not make |LayoutRubyAsBlock| as inline contents container,
    // because ruby base text comes after ruby text in layout tree.
    // See ParameterizedTextOffsetMappingTest.RangeOfBlockWithRubyAsBlock
    return false;
  }
  if (block_flow->NonPseudoNode()) {
    // It is OK as long as |block_flow| is associated to non-pseudo |Node| even
    // if it is empty block or containing only anonymous objects.
    // See LinkSelectionClickEventsTest.SingleAndDoubleClickWillBeHandled
    return true;
  }
  // Since we can't create |EphemeralRange|, we exclude a |LayoutBlockFlow| if
  // its entire subtree is anonymous, e.g. |LayoutMultiColumnSet|,
  // and with anonymous layout objects.
  return HasNonPsuedoNode(*block_flow);
}

Node* PreviousNodeSkippingAncestors(const Node& node) {
  ContainerNode* parent = FlatTreeTraversal::Parent(node);
  for (Node* runner = FlatTreeTraversal::Previous(node); runner;
       runner = FlatTreeTraversal::Previous(*runner)) {
    if (runner != parent)
      return runner;
    parent = FlatTreeTraversal::Parent(*runner);
  }
  return nullptr;
}

// Returns outer most nested inline formatting context.
const LayoutBlockFlow& RootInlineContentsContainerOf(
    const LayoutBlockFlow& block_flow) {
  DCHECK(block_flow.ChildrenInline()) << block_flow;
  const LayoutBlockFlow* root_block_flow = &block_flow;
  for (const LayoutBlock* runner = block_flow.ContainingBlock(); runner;
       runner = runner->ContainingBlock()) {
    auto* containing_block_flow = DynamicTo<LayoutBlockFlow>(runner);
    if (!containing_block_flow || !runner->ChildrenInline())
      break;
    root_block_flow = containing_block_flow;
  }
  DCHECK(!root_block_flow->IsAtomicInlineLevel())
      << block_flow << ' ' << root_block_flow;
  return *root_block_flow;
}

bool ShouldSkipChildren(const Node& node) {
  if (IsTextControl(node))
    return true;
  const ShadowRoot* const root = node.GetShadowRoot();
  return root && root->IsUserAgent();
}

LayoutObject* NextForInlineContents(const LayoutObject& layout_object,
                                    const LayoutObject& container) {
  if (layout_object.IsBlockInInline())
    return layout_object.NextInPreOrderAfterChildren(&container);
  const Node* const node = layout_object.NonPseudoNode();
  if (node && ShouldSkipChildren(*node))
    return layout_object.NextInPreOrderAfterChildren(&container);
  return layout_object.NextInPreOrder(&container);
}

const Node* FindFirstNonPseudoNodeIn(const LayoutObject& container) {
  for (const LayoutObject* layout_object = container.SlowFirstChild();
       layout_object;
       layout_object = NextForInlineContents(*layout_object, container)) {
    if (auto* node = layout_object->NonPseudoNode())
      return node;
  }
  return nullptr;
}

const Node* FindLastNonPseudoNodeIn(const LayoutObject& container) {
  const Node* last_node = nullptr;
  for (const LayoutObject* layout_object = container.SlowFirstChild();
       layout_object;
       layout_object = NextForInlineContents(*layout_object, container)) {
    if (auto* node = layout_object->NonPseudoNode())
      last_node = node;
  }
  return last_node;
}

// TODO(editing-dev): We should have |ComputeInlineContents()| computing first
// and last layout objects representing a run of inline layout objects in
// |LayoutBlockFlow| instead of using |ComputeInlineContentsAsBlockFlow()|.
//
// For example "<p>a<b>CD<p>EF</p>G</b>h</p>", where b has display:inline-block.
// We should have three ranges:
//  1. aCD
//  2. EF
//  3. Gh
// See RangeWithNestedInlineBlock* tests.

// Note: Since "inline-block" and "float" are not considered as text segment
// boundary, we should not consider them as block for scanning.
// Example in selection text:
//  <div>|ab<b style="display:inline-block">CD</b>ef</div>
//  selection.modify('extent', 'forward', 'word')
//  <div>^ab<b style="display:inline-block">CD</b>ef|</div>
// See also test cases for "inline-block" and "float" in |TextIterator|
//
// This is a helper function to compute inline layout object run from
// |LayoutBlockFlow|.
const LayoutBlockFlow* ComputeInlineContentsAsBlockFlow(
    const LayoutObject& layout_object) {
  const auto* block = DynamicTo<LayoutBlock>(layout_object);
  if (!block)
    block = layout_object.ContainingBlock();

  DCHECK(block) << layout_object;
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(block);
  if (!block_flow)
    return nullptr;
  if (!block_flow->ChildrenInline())
    return nullptr;
  if (block_flow->IsAtomicInlineLevel() ||
      block_flow->IsFloatingOrOutOfFlowPositioned()) {
    const LayoutBlockFlow& root_block_flow =
        RootInlineContentsContainerOf(*block_flow);
    // Skip |root_block_flow| if it's an anonymous wrapper created for
    // pseudo elements. See test AnonymousBlockFlowWrapperForFloatPseudo.
    if (!CanBeInlineContentsContainer(root_block_flow))
      return nullptr;
    return &root_block_flow;
  }
  if (!CanBeInlineContentsContainer(*block_flow))
    return nullptr;
  return block_flow;
}

TextOffsetMapping::InlineContents CreateInlineContentsFromBlockFlow(
    const LayoutBlockFlow& block_flow,
    const LayoutObject& target) {
  DCHECK(block_flow.ChildrenInline()) << block_flow;
  DCHECK(target.NonPseudoNode()) << target;
  const LayoutObject* layout_object = nullptr;
  const LayoutObject* block_in_inline_before = nullptr;
  const LayoutObject* first = nullptr;
  const LayoutObject* last = nullptr;
  for (layout_object = block_flow.FirstChild(); layout_object;
       layout_object = NextForInlineContents(*layout_object, block_flow)) {
    if (layout_object->NonPseudoNode()) {
      last = layout_object;
      if (!first)
        first = layout_object;
    }
    if (layout_object == &target) {
      // Note: When |target| is in subtree of user agent shadow root, we don't
      // reach here. See  http://crbug.com/1224206
      last = first;
      break;
    }
    if (layout_object->IsBlockInInline()) {
      if (target.IsDescendantOf(layout_object)) {
        // Note: We reach here when `target` is `position:absolute` or
        // `position:fixed`, aka `IsOutOfFlowPositioned()`, because
        // `LayoutObject::ContainingBlock()` handles them specially.
        // See http://crbug.com/1324970
        last = first;
        break;
      }
      block_in_inline_before = layout_object;
      first = last = nullptr;
    }
  }
  if (!first) {
    DCHECK(block_flow.NonPseudoNode()) << block_flow;
    return TextOffsetMapping::InlineContents(block_flow);
  }
  const LayoutObject* block_in_inline_after = nullptr;
  for (; layout_object;
       layout_object = NextForInlineContents(*layout_object, block_flow)) {
    if (layout_object->IsBlockInInline()) {
      block_in_inline_after = layout_object;
      break;
    }
    if (layout_object->NonPseudoNode()) {
      last = layout_object;
    }
  }
  DCHECK(last);
  return TextOffsetMapping::InlineContents(
      block_flow, block_in_inline_before, *first, *last, block_in_inline_after);
}

TextOffsetMapping::InlineContents ComputeInlineContentsFromNode(
    const Node& node) {
  const LayoutObject* const layout_object = node.GetLayoutObject();
  if (!layout_object)
    return TextOffsetMapping::InlineContents();
  const LayoutBlockFlow* const block_flow =
      ComputeInlineContentsAsBlockFlow(*layout_object);
  if (!block_flow)
    return TextOffsetMapping::InlineContents();
  return CreateInlineContentsFromBlockFlow(*block_flow, *layout_object);
}

String Ensure16Bit(const String& text) {
  String text16(text);
  text16.Ensure16Bit();
  return text16;
}

}  // namespace

TextOffsetMapping::TextOffsetMapping(const InlineContents& inline_contents,
                                     const TextIteratorBehavior& behavior)
    : behavior_(behavior),
      range_(inline_contents.GetRange()),
      text16_(Ensure16Bit(PlainText(range_, behavior_))) {}

TextOffsetMapping::TextOffsetMapping(const InlineContents& inline_contents)
    : TextOffsetMapping(inline_contents,
                        TextIteratorBehavior::Builder()
                            .SetEmitsCharactersBetweenAllVisiblePositions(true)
                            .SetEmitsSmallXForTextSecurity(true)
                            .Build()) {}

int TextOffsetMapping::ComputeTextOffset(
    const PositionInFlatTree& position) const {
  if (position <= range_.StartPosition())
    return 0;
  if (position >= range_.EndPosition())
    return text16_.length();
  return TextIteratorInFlatTree::RangeLength(range_.StartPosition(), position,
                                             behavior_);
}

PositionInFlatTree TextOffsetMapping::GetPositionBefore(unsigned offset) const {
  DCHECK_LE(offset, text16_.length());
  CharacterIteratorInFlatTree iterator(range_, behavior_);
  if (offset >= 1 && offset == text16_.length()) {
    iterator.Advance(offset - 1);
    return iterator.GetPositionAfter();
  }
  iterator.Advance(offset);
  return iterator.GetPositionBefore();
}

PositionInFlatTree TextOffsetMapping::GetPositionAfter(unsigned offset) const {
  DCHECK_LE(offset, text16_.length());
  CharacterIteratorInFlatTree iterator(range_, behavior_);
  iterator.Advance(offset);
  return iterator.GetPositionAfter();
}

EphemeralRangeInFlatTree TextOffsetMapping::ComputeRange(unsigned start,
                                                         unsigned end) const {
  DCHECK_LE(end, text16_.length());
  DCHECK_LE(start, end);
  if (start == end)
    return EphemeralRangeInFlatTree();
  return EphemeralRangeInFlatTree(GetPositionBefore(start),
                                  GetPositionAfter(end));
}

unsigned TextOffsetMapping::FindNonWhitespaceCharacterFrom(
    unsigned offset) const {
  for (unsigned runner = offset; runner < text16_.length(); ++runner) {
    if (!IsWhitespace(text16_[runner]))
      return runner;
  }
  return text16_.length();
}

// static
TextOffsetMapping::BackwardRange TextOffsetMapping::BackwardRangeOf(
    const PositionInFlatTree& position) {
  return BackwardRange(FindBackwardInlineContents(position));
}

// static
TextOffsetMapping::ForwardRange TextOffsetMapping::ForwardRangeOf(
    const PositionInFlatTree& position) {
  return ForwardRange(FindForwardInlineContents(position));
}

// static
template <typename Traverser>
TextOffsetMapping::InlineContents TextOffsetMapping::FindInlineContentsInternal(
    const Node* start_node,
    Traverser traverser) {
  for (const Node* node = start_node; node; node = traverser(*node)) {
    const InlineContents inline_contents = ComputeInlineContentsFromNode(*node);
    if (inline_contents.IsNotNull())
      return inline_contents;
  }
  return InlineContents();
}

// static
TextOffsetMapping::InlineContents TextOffsetMapping::FindBackwardInlineContents(
    const PositionInFlatTree& position) {
  const Node* previous_node = position.NodeAsRangeLastNode();
  if (!previous_node)
    return InlineContents();

  if (const TextControlElement* enclosing_text_control =
          EnclosingTextControl(position)) {
    if (!FlatTreeTraversal::IsDescendantOf(*previous_node,
                                           *enclosing_text_control)) {
      // The first position in a text control reaches here.
      return InlineContents();
    }

    return TextOffsetMapping::FindInlineContentsInternal(
        previous_node, [enclosing_text_control](const Node& node) {
          return FlatTreeTraversal::Previous(node, enclosing_text_control);
        });
  }

  auto previous_skipping_text_control = [](const Node& node) -> const Node* {
    DCHECK(!EnclosingTextControl(&node));
    const Node* previous = PreviousNodeSkippingAncestors(node);
    if (!previous)
      return previous;
    const TextControlElement* previous_text_control =
        EnclosingTextControl(previous);
    if (previous_text_control)
      return previous_text_control;
    if (ShadowRoot* root = previous->ContainingShadowRoot()) {
      if (root->IsUserAgent())
        return root->OwnerShadowHost();
    }
    return previous;
  };

  if (const TextControlElement* last_enclosing_text_control =
          EnclosingTextControl(previous_node)) {
    // Example, <input value=foo><span>bar</span>, span@beforeAnchor
    return TextOffsetMapping::FindInlineContentsInternal(
        last_enclosing_text_control, previous_skipping_text_control);
  }
  return TextOffsetMapping::FindInlineContentsInternal(
      previous_node, previous_skipping_text_control);
}

// static
// Note: "doubleclick-whitespace-img-crash.html" call |NextWordPosition())
// with AfterNode(IMG) for <body><img></body>
TextOffsetMapping::InlineContents TextOffsetMapping::FindForwardInlineContents(
    const PositionInFlatTree& position) {
  const Node* next_node = position.NodeAsRangeFirstNode();
  if (!next_node)
    return InlineContents();

  if (const TextControlElement* enclosing_text_control =
          EnclosingTextControl(position)) {
    if (!FlatTreeTraversal::IsDescendantOf(*next_node,
                                           *enclosing_text_control)) {
      // The last position in a text control reaches here.
      return InlineContents();
    }

    return TextOffsetMapping::FindInlineContentsInternal(
        next_node, [enclosing_text_control](const Node& node) {
          return FlatTreeTraversal::Next(node, enclosing_text_control);
        });
  }

  auto next_skipping_text_control = [](const Node& node) {
    DCHECK(!EnclosingTextControl(&node));
    if (ShouldSkipChildren(node))
      return FlatTreeTraversal::NextSkippingChildren(node);
    return FlatTreeTraversal::Next(node);
  };
  DCHECK(!EnclosingTextControl(next_node));
  return TextOffsetMapping::FindInlineContentsInternal(
      next_node, next_skipping_text_control);
}

// ----

TextOffsetMapping::InlineContents::InlineContents(
    const LayoutBlockFlow& block_flow)
    : block_flow_(&block_flow) {
  DCHECK(block_flow_->NonPseudoNode());
  DCHECK(CanBeInlineContentsContainer(*block_flow_)) << block_flow_;
}

// |first| and |last| should not be anonymous object.
// Note: "extend_selection_10_ltr_backward_word.html" has a block starts with
// collapsible whitespace with anonymous object.
TextOffsetMapping::InlineContents::InlineContents(
    const LayoutBlockFlow& block_flow,
    const LayoutObject* block_in_inline_before,
    const LayoutObject& first,
    const LayoutObject& last,
    const LayoutObject* block_in_inline_after)
    : block_flow_(&block_flow),
      block_in_inline_before_(block_in_inline_before),
      first_(&first),
      last_(&last),
      block_in_inline_after_(block_in_inline_after) {
  DCHECK(!block_in_inline_before_ || block_in_inline_before_->IsBlockInInline())
      << block_in_inline_before_;
  DCHECK(!block_in_inline_after_ || block_in_inline_after_->IsBlockInInline())
      << block_in_inline_after_;
  DCHECK(first_->NonPseudoNode()) << first_;
  DCHECK(last_->NonPseudoNode()) << last_;
  DCHECK(CanBeInlineContentsContainer(*block_flow_)) << block_flow_;
  DCHECK(first_->IsDescendantOf(block_flow_));
  DCHECK(last_->IsDescendantOf(block_flow_));
}

bool TextOffsetMapping::InlineContents::operator==(
    const InlineContents& other) const {
  return block_flow_ == other.block_flow_;
}

const LayoutBlockFlow* TextOffsetMapping::InlineContents::GetEmptyBlock()
    const {
  DCHECK(block_flow_ && !first_ && !last_);
  return block_flow_;
}

const LayoutObject& TextOffsetMapping::InlineContents::FirstLayoutObject()
    const {
  DCHECK(first_);
  return *first_;
}

const LayoutObject& TextOffsetMapping::InlineContents::LastLayoutObject()
    const {
  DCHECK(last_);
  return *last_;
}

EphemeralRangeInFlatTree TextOffsetMapping::InlineContents::GetRange() const {
  DCHECK(block_flow_);
  if (!first_) {
    const Node& node = *block_flow_->NonPseudoNode();
    return EphemeralRangeInFlatTree(
        PositionInFlatTree::FirstPositionInNode(node),
        PositionInFlatTree::LastPositionInNode(node));
  }
  const Node& first_node = *first_->NonPseudoNode();
  const Node& last_node = *last_->NonPseudoNode();
  auto* first_text_node = DynamicTo<Text>(first_node);
  auto* last_text_node = DynamicTo<Text>(last_node);
  return EphemeralRangeInFlatTree(
      first_text_node ? PositionInFlatTree(first_node, 0)
                      : PositionInFlatTree::BeforeNode(first_node),
      last_text_node ? PositionInFlatTree(last_node, last_text_node->length())
                     : PositionInFlatTree::AfterNode(last_node));
}

PositionInFlatTree
TextOffsetMapping::InlineContents::LastPositionBeforeBlockFlow() const {
  DCHECK(block_flow_);
  if (block_in_inline_before_) {
    for (const LayoutObject* block = block_in_inline_before_->SlowLastChild();
         block; block = block->PreviousSibling()) {
      if (auto* block_node = block->NonPseudoNode())
        return PositionInFlatTree::LastPositionInNode(*block_node);
      if (auto* last_node = FindLastNonPseudoNodeIn(*block)) {
        return PositionInFlatTree::AfterNode(*last_node);
      }
    }
  }
  if (const Node* node = block_flow_->NonPseudoNode()) {
    if (!FlatTreeTraversal::Parent(*node)) {
      // Reached start of document.
      return PositionInFlatTree();
    }
    return PositionInFlatTree::BeforeNode(*node);
  }
  DCHECK(first_);
  DCHECK(first_->NonPseudoNode());
  DCHECK(FlatTreeTraversal::Parent(*first_->NonPseudoNode()));
  return PositionInFlatTree::BeforeNode(*first_->NonPseudoNode());
}

PositionInFlatTree
TextOffsetMapping::InlineContents::FirstPositionAfterBlockFlow() const {
  DCHECK(block_flow_);
  if (block_in_inline_after_) {
    for (const LayoutObject* block = block_in_inline_after_->SlowFirstChild();
         block; block = block->NextSibling()) {
      if (auto* block_node = block->NonPseudoNode())
        return PositionInFlatTree::BeforeNode(*block_node);
      if (auto* first_node = FindFirstNonPseudoNodeIn(*block)) {
        return PositionInFlatTree::BeforeNode(*first_node);
      }
    }
  }
  if (const Node* node = block_flow_->NonPseudoNode()) {
    if (!FlatTreeTraversal::Parent(*node)) {
      // Reached end of document.
      return PositionInFlatTree();
    }
    return PositionInFlatTree::AfterNode(*node);
  }
  DCHECK(last_);
  DCHECK(last_->NonPseudoNode());
  DCHECK(FlatTreeTraversal::Parent(*last_->NonPseudoNode()));
  return PositionInFlatTree::AfterNode(*last_->NonPseudoNode());
}

// static
TextOffsetMapping::InlineContents TextOffsetMapping::InlineContents::NextOf(
    const InlineContents& inline_contents) {
  const PositionInFlatTree position_after =
      inline_contents.FirstPositionAfterBlockFlow();
  if (position_after.IsNull())
    return InlineContents();
  return TextOffsetMapping::FindForwardInlineContents(position_after);
}

// static
TextOffsetMapping::InlineContents TextOffsetMapping::InlineContents::PreviousOf(
    const InlineContents& inline_contents) {
  const PositionInFlatTree position_before =
      inline_contents.LastPositionBeforeBlockFlow();
  if (position_before.IsNull())
    return InlineContents();
  return TextOffsetMapping::FindBackwardInlineContents(position_before);
}

std::ostream& operator<<(
    std::ostream& ostream,
    const TextOffsetMapping::InlineContents& inline_contents) {
  return ostream << '[' << inline_contents.FirstLayoutObject() << ", "
                 << inline_contents.LastLayoutObject() << ']';
}

// ----

TextOffsetMapping::InlineContents TextOffsetMapping::BackwardRange::Iterator::
operator*() const {
  DCHECK(current_.IsNotNull());
  return current_;
}

void TextOffsetMapping::BackwardRange::Iterator::operator++() {
  DCHECK(current_.IsNotNull());
  current_ = TextOffsetMapping::InlineContents::PreviousOf(current_);
}

// ----

TextOffsetMapping::InlineContents TextOffsetMapping::ForwardRange::Iterator::
operator*() const {
  DCHECK(current_.IsNotNull());
  return current_;
}

void TextOffsetMapping::ForwardRange::Iterator::operator++() {
  DCHECK(current_.IsNotNull());
  current_ = TextOffsetMapping::InlineContents::NextOf(current_);
}

}  // namespace blink
