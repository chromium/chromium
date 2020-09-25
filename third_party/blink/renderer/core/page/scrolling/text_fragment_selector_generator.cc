// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"

namespace blink {

namespace {

// Returns text within given positions of the node, skipping invisible children
// and comments.
String GetText(Node* node, int start_position, int end_position) {
  auto range_start = Position(node, start_position);
  auto range_end = Position(node, end_position);
  return PlainText(EphemeralRange(range_start, range_end));
}

// Returns text content of the node, skipping invisible children and comments.
String GetText(Node* node) {
  return PlainText(EphemeralRange::RangeOfContents(*node));
}

// Returns true if text from beginning of |node| until |pos_offset| can be
// considered empty. Otherwise, return false.
bool IsFirstVisiblePosition(Node* node, unsigned pos_offset) {
  return pos_offset == 0 ||
         GetText(node, 0, pos_offset).StripWhiteSpace().IsEmpty();
}

// Returns true if text from |pos_offset| until end of |node| can be considered
// empty. Otherwise, return false.
bool IsLastVisiblePosition(Node* node, unsigned pos_offset) {
  return pos_offset == node->textContent().length() ||
         GetText(node, pos_offset, node->textContent().length())
             .StripWhiteSpace()
             .IsEmpty();
}
struct ForwadDirection {
  static Node* Next(const Node& node) { return FlatTreeTraversal::Next(node); }
  static Node* Next(const Node& node, const Node* stay_within) {
    return FlatTreeTraversal::Next(node, stay_within);
  }
  static Node* GetVisibleTextNode(Node& start_node) {
    return FindBuffer::ForwardVisibleTextNode(start_node);
  }
};

struct BackwardDirection {
  static Node* Next(const Node& node) {
    return FlatTreeTraversal::Previous(node);
  }
  static Node* Next(const Node& node, const Node* stay_within) {
    return FlatTreeTraversal::Previous(node, stay_within);
  }
  static Node* GetVisibleTextNode(Node& start_node) {
    return FindBuffer::BackwardVisibleTextNode(start_node);
  }
};

template <class Direction>
Node* NextNonEmptyVisibleTextNode(Node* start_node) {
  if (!start_node)
    return nullptr;

  // Move forward/backward until non empty visible text node is found.
  for (Node* node = start_node; node; node = Direction::Next(*node)) {
    Node* next_node = Direction::GetVisibleTextNode(*node);
    if (!next_node || !GetText(next_node).IsEmpty())
      return next_node;
    node = next_node;
  }
  return nullptr;
}

// Returns the next/previous visible node to |start_node|.
Node* FirstNonEmptyVisibleTextNode(Node* start_node) {
  return NextNonEmptyVisibleTextNode<ForwadDirection>(start_node);
}

Node* BackwardNonEmptyVisibleTextNode(Node* start_node) {
  return NextNonEmptyVisibleTextNode<BackwardDirection>(start_node);
}

// Returns the furthest node within same block as |start_node| without crossing
// block boundaries.
// Returns the next/previous visible node to |start_node|.
template <class Direction>
Node* FurthestVisibleTextNodeWithinBlock(Node* start_node) {
  Node& block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(*start_node);

  // Move forward/backward until no next/previous node is available within same
  // |block_ancestor|.
  Node* last_node;
  for (Node* node = start_node; node;
       node = Direction::Next(*node, &block_ancestor)) {
    node = Direction::GetVisibleTextNode(*node);
    // Stop, if crossed block boundaries.
    if (!node ||
        !FindBuffer::GetFirstBlockLevelAncestorInclusive(*node).isSameNode(
            &block_ancestor))
      break;
    last_node = node;
  }
  return last_node;
}

Node* FirstVisibleTextNodeWithinBlock(Node* start_node) {
  return FurthestVisibleTextNodeWithinBlock<ForwadDirection>(start_node);
}

Node* LastVisibleTextNodeWithinBlock(Node* start_node) {
  return FurthestVisibleTextNodeWithinBlock<BackwardDirection>(start_node);
}

// Returns whitespace-trimmed substring of |text| containing the final
// |word_num| words.
String GetWordsFromEnd(String text, int word_num) {
  if (text.IsEmpty())
    return "";

  int pos = text.length() - 1;
  text.Ensure16Bit();
  while (word_num-- > 0)
    pos = FindNextWordBackward(text.Characters16(), text.length(), pos);

  return text.Substring(pos, text.length()).StripWhiteSpace();
}

// Returns whitespace-trimmed substring of |text| containing the first
// |word_num| words.
String GetWordsFromStart(String text, int word_num) {
  if (text.IsEmpty())
    return "";

  int pos = 0;
  text.Ensure16Bit();
  while (word_num-- > 0)
    pos = FindNextWordForward(text.Characters16(), text.length(), pos);

  return text.Substring(0, pos).StripWhiteSpace();
}

}  // namespace

constexpr int kExactTextMaxChars = 300;
constexpr int kNoContextMinChars = 20;
constexpr int kMaxContextWords = 10;
constexpr int kMaxRangeWords = 10;

void TextFragmentSelectorGenerator::UpdateSelection(
    LocalFrame* selection_frame,
    const EphemeralRangeInFlatTree& selection_range) {
  DCHECK(selection_frame);

  // Scroll-to-text doesn't support iframes.
  DCHECK(selection_frame->IsMainFrame());

  selection_frame_ = selection_frame;
  selection_range_ = MakeGarbageCollected<Range>(
      selection_range.GetDocument(),
      ToPositionInDOMTree(selection_range.StartPosition()),
      ToPositionInDOMTree(selection_range.EndPosition()));
}

void TextFragmentSelectorGenerator::BindTextFragmentSelectorProducer(
    mojo::PendingReceiver<mojom::blink::TextFragmentSelectorProducer>
        producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      selection_frame_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void TextFragmentSelectorGenerator::CompleteSelection() {
  if (!selection_range_)
    return;

  EphemeralRangeInFlatTree ephemeral_range(selection_range_);
  Node* start_container =
      ephemeral_range.StartPosition().ComputeContainerNode();
  Node* end_container = ephemeral_range.EndPosition().ComputeContainerNode();

  String start_text = start_container->textContent();
  int selection_start_pos =
      ephemeral_range.StartPosition().ComputeOffsetInContainerNode();
  start_text.Ensure16Bit();
  int word_start = FindWordStartBoundary(
      start_text.Characters16(), start_text.length(), selection_start_pos);

  String end_text = end_container->textContent();
  int selection_end_pos =
      ephemeral_range.EndPosition().ComputeOffsetInContainerNode();
  end_text.Ensure16Bit();
  int word_end = FindWordEndBoundary(end_text.Characters16(), end_text.length(),
                                     selection_end_pos);
  if (word_start != selection_start_pos || word_end != selection_end_pos) {
    selection_range_ =
        MakeGarbageCollected<Range>(selection_range_->OwnerDocument(),
                                    Position(start_container, word_start),
                                    Position(end_container, word_end));
  }
}

void TextFragmentSelectorGenerator::GenerateSelector(
    GenerateSelectorCallback callback) {
  DCHECK(selection_range_);
  DCHECK(callback);

  pending_generate_selector_callback_ = std::move(callback);
  state_ = kNeedsNewCandidate;
  step_ = kExact;
  max_available_prefix_ = "";
  max_available_suffix_ = "";
  num_prefix_words_ = 0;
  num_suffix_words_ = 0;

  CompleteSelection();
  GenerateSelectorCandidate();
}

void TextFragmentSelectorGenerator::GenerateSelectorCandidate() {
  DCHECK_EQ(kNeedsNewCandidate, state_);

  if (step_ == kExact)
    GenerateExactSelector();

  if (step_ == kRange)
    ExtendRangeSelector();

  if (step_ == kContext)
    ExtendContext();
  ResolveSelectorState();
}

void TextFragmentSelectorGenerator::ResolveSelectorState() {
  switch (state_) {
    case kTestCandidate:
      RunTextFinder();
      break;
    case kNeedsNewCandidate:
      NOTREACHED();
      ABSL_FALLTHROUGH_INTENDED;
    case kFailure:
      NotifySelectorReady(
          TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
      break;
    case kSuccess:
      NotifySelectorReady(*selector_);
      break;
  }
}

void TextFragmentSelectorGenerator::RunTextFinder() {
  DCHECK(selector_);

  // |FindMatch| will call |DidFindMatch| indicating if the match was unique.
  TextFragmentFinder finder(*this, *selector_);
  finder.FindMatch(*selection_frame_->GetDocument());
}

void TextFragmentSelectorGenerator::DidFindMatch(
    const EphemeralRangeInFlatTree& match,
    const TextFragmentAnchorMetrics::Match match_metrics,
    bool is_unique) {
  if (is_unique && PlainText(match).StripWhiteSpace().length() ==
                       PlainText(EphemeralRange(selection_range_))
                           .StripWhiteSpace()
                           .length()) {
    state_ = kSuccess;
    ResolveSelectorState();
  } else {
    state_ = kNeedsNewCandidate;

    // If already tried exact selector then should continue by adding context.
    if (step_ == kExact)
      step_ = kContext;
    GenerateSelectorCandidate();
  }
}

void TextFragmentSelectorGenerator::NotifySelectorReady(
    const TextFragmentSelector& selector) {
  DCHECK(pending_generate_selector_callback_);
  std::move(pending_generate_selector_callback_).Run(selector.ToString());
}

void TextFragmentSelectorGenerator::ClearSelection() {
  if (selection_range_) {
    selection_range_->Dispose();
    selection_range_ = nullptr;
    selection_frame_ = nullptr;
  }
}

void TextFragmentSelectorGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(selection_frame_);
  visitor->Trace(selection_range_);
  visitor->Trace(selector_producer_);
}

void TextFragmentSelectorGenerator::GenerateExactSelector() {
  DCHECK_EQ(kExact, step_);
  DCHECK_EQ(kNeedsNewCandidate, state_);

  EphemeralRangeInFlatTree ephemeral_range(selection_range_);
  Node* start_container =
      ephemeral_range.StartPosition().ComputeContainerNode();
  Node* end_container = ephemeral_range.EndPosition().ComputeContainerNode();

  Node& start_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(*start_container);
  Node& end_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(*end_container);

  // If not in same node, should use ranges.
  if (!start_first_block_ancestor.isSameNode(&end_first_block_ancestor)) {
    step_ = kRange;
    return;
  }

  // TODO(gayane): If same node, need to check if start and end are interrupted
  // by a block. Example: <div>start of the selection <div> sub block </div>end
  // of the selection</div>.

  String selected_text = PlainText(ephemeral_range);

  // If too long should use ranges.
  if (selected_text.length() > kExactTextMaxChars) {
    step_ = kRange;
    return;
  }

  selector_ = std::make_unique<TextFragmentSelector>(
      TextFragmentSelector::SelectorType::kExact,
      selected_text.StripWhiteSpace(), "", "", "");

  // If too short should use exact selector, but should add context.
  if (selected_text.length() < kNoContextMinChars) {
    step_ = kContext;
    return;
  }

  state_ = kTestCandidate;
}

void TextFragmentSelectorGenerator::ExtendRangeSelector() {
  DCHECK_EQ(kRange, step_);
  DCHECK_EQ(kNeedsNewCandidate, state_);

  // Give up if range is already too long.
  if (num_range_start_words_ == kMaxRangeWords ||
      num_range_end_words_ == kMaxRangeWords) {
    step_ = kContext;
    return;
  }

  // Initialize range start and end, if needed.
  if (max_available_range_start_.IsEmpty() &&
      max_available_range_end_.IsEmpty()) {
    EphemeralRangeInFlatTree ephemeral_range(selection_range_);

    Node& start_first_block_ancestor =
        FindBuffer::GetFirstBlockLevelAncestorInclusive(
            *ephemeral_range.StartPosition().ComputeContainerNode());
    Node& end_first_block_ancestor =
        FindBuffer::GetFirstBlockLevelAncestorInclusive(
            *ephemeral_range.EndPosition().ComputeContainerNode());

    // If selection starts and ends in the same block, then split selected text
    // roughly in the middle.
    // TODO(gayane): Should also check that there are no nested blocks.
    if (start_first_block_ancestor.isSameNode(&end_first_block_ancestor)) {
      String selection_text = PlainText(ephemeral_range);
      selection_text.Ensure16Bit();
      int selection_length = selection_text.length();
      int mid_point =
          FindNextWordForward(selection_text.Characters16(), selection_length,
                              selection_length / 2);
      max_available_range_start_ = selection_text.Left(mid_point);

      // If from middle till end of selection there is no word break, then we
      // cannot use it for range end.
      if (mid_point == selection_length) {
        state_ = kFailure;
        return;
      }

      max_available_range_end_ =
          selection_text.Right(selection_text.length() - mid_point - 1);
    } else {
      // If not the same node, then we use first and last block of the selection
      // range.
      max_available_range_start_ =
          GetNextTextBlock(selection_range_->StartPosition());
      max_available_range_end_ =
          GetPreviousTextBlock(selection_range_->EndPosition());
    }
  }

  String start =
      GetWordsFromStart(max_available_range_start_, ++num_range_start_words_);
  String end =
      GetWordsFromEnd(max_available_range_end_, ++num_range_end_words_);

  // If the start and end didn't change, it means we exhausted the selected
  // text and should try adding context.
  if (selector_ && start == selector_->Start() && end == selector_->End()) {
    step_ = kContext;
    return;
  }
  selector_ = std::make_unique<TextFragmentSelector>(
      TextFragmentSelector::SelectorType::kRange, start, end, "", "");
  state_ = kTestCandidate;
}

void TextFragmentSelectorGenerator::ExtendContext() {
  DCHECK_EQ(kContext, step_);
  DCHECK_EQ(kNeedsNewCandidate, state_);
  DCHECK(selector_);

  // Give up if context is already too long.
  if (num_prefix_words_ == kMaxContextWords ||
      num_prefix_words_ == kMaxContextWords) {
    state_ = kFailure;
    return;
  }

  // Try initiating properties necessary for calculating prefix and suffix.
  if (max_available_prefix_.IsEmpty() && max_available_suffix_.IsEmpty()) {
    max_available_prefix_ =
        GetPreviousTextBlock(selection_range_->StartPosition());
    max_available_suffix_ = GetNextTextBlock(selection_range_->EndPosition());
  }

  if (max_available_prefix_.IsEmpty() && max_available_suffix_.IsEmpty()) {
    state_ = kFailure;
    return;
  }

  String prefix = GetWordsFromEnd(max_available_prefix_, ++num_prefix_words_);
  String suffix = GetWordsFromStart(max_available_suffix_, ++num_suffix_words_);

  // Give up if we were unable to get new prefix and suffix.
  if (prefix == selector_->Prefix() && suffix == selector_->Suffix()) {
    state_ = kFailure;
    return;
  }
  selector_ = std::make_unique<TextFragmentSelector>(
      selector_->Type(), selector_->Start(), selector_->End(), prefix, suffix);

  state_ = kTestCandidate;
}

String TextFragmentSelectorGenerator::GetPreviousTextBlock(
    const Position& prefix_end_position) {
  Node* prefix_end = prefix_end_position.ComputeContainerNode();
  unsigned prefix_end_offset =
      prefix_end_position.ComputeOffsetInContainerNode();

  // If given position point to the first visible text in its containiner node,
  // use the preceding visible node for the suffix.
  if (IsFirstVisiblePosition(prefix_end, prefix_end_offset)) {
    prefix_end = BackwardNonEmptyVisibleTextNode(
        FlatTreeTraversal::PreviousSkippingChildren(*prefix_end));

    if (!prefix_end)
      return "";
    prefix_end_offset = prefix_end->textContent().length();
  }

  // The furthest node within same block without crossing block boundaries would
  // be the suffix end.
  Node* prefix_start = LastVisibleTextNodeWithinBlock(prefix_end);
  if (!prefix_start)
    return "";

  auto range_start = Position(prefix_start, 0);
  auto range_end = Position(prefix_end, prefix_end_offset);
  return PlainText(EphemeralRange(range_start, range_end)).StripWhiteSpace();
}

String TextFragmentSelectorGenerator::GetNextTextBlock(
    const Position& suffix_start_position) {
  Node* suffix_start = suffix_start_position.ComputeContainerNode();
  unsigned suffix_start_offset =
      suffix_start_position.ComputeOffsetInContainerNode();

  // If given position point to the last visible text in its containiner node,
  // use the following visible node for the suffix.
  if (IsLastVisiblePosition(suffix_start, suffix_start_offset)) {
    suffix_start = FirstNonEmptyVisibleTextNode(
        FlatTreeTraversal::NextSkippingChildren(*suffix_start));
    suffix_start_offset = 0;
  }
  if (!suffix_start)
    return "";

  // The furthest node within same block without crossing block boundaries would
  // be the suffix end.
  Node* suffix_end = FirstVisibleTextNodeWithinBlock(suffix_start);
  if (!suffix_end)
    return "";

  auto range_start = Position(suffix_start, suffix_start_offset);
  auto range_end = Position(suffix_end, suffix_end->textContent().length());
  return PlainText(EphemeralRange(range_start, range_end)).StripWhiteSpace();
}
}  // namespace blink
