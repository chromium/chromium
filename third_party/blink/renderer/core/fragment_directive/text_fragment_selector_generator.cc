// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"

using LinkGenerationError = shared_highlighting::LinkGenerationError;

namespace blink {

namespace {

// Returns true if text from beginning of |node| until |pos_offset| can be
// considered empty. Otherwise, return false.
bool IsFirstVisiblePosition(Node* node, unsigned pos_offset) {
  auto range_start = PositionInFlatTree::FirstPositionInNode(*node);
  auto range_end = PositionInFlatTree(node, pos_offset);
  return node->getNodeType() == Node::kElementNode || pos_offset == 0 ||
         PlainText(EphemeralRangeInFlatTree(range_start, range_end))
             .StripWhiteSpace()
             .IsEmpty();
}

// Returns true if text from |pos_offset| until end of |node| can be considered
// empty. Otherwise, return false.
bool IsLastVisiblePosition(Node* node, unsigned pos_offset) {
  auto range_start = PositionInFlatTree(node, pos_offset);
  auto range_end = PositionInFlatTree::LastPositionInNode(*node);
  return node->getNodeType() == Node::kElementNode ||
         pos_offset == node->textContent().length() ||
         PlainText(EphemeralRangeInFlatTree(range_start, range_end))
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
  // |IsInSameUninterruptedBlock| is diraction specific because |start| and
  // |end| should be in right order.
  static bool IsInSameUninterruptedBlock(Node& start, Node& end) {
    return FindBuffer::IsInSameUninterruptedBlock(start, end);
  }
};

struct BackwardDirection {
  static Node* Next(const Node& node) {
    return FlatTreeTraversal::Previous(node);
  }
  static Node* GetVisibleTextNode(Node& start_node) {
    return FindBuffer::BackwardVisibleTextNode(start_node);
  }
  // |IsInSameUninterruptedBlock| is diraction specific because |start| and
  // |end| should be in right order.
  static bool IsInSameUninterruptedBlock(Node& start, Node& end) {
    return FindBuffer::IsInSameUninterruptedBlock(end, start);
  }
};

template <class Direction>
Node* NextNonEmptyVisibleTextNode(Node* start_node) {
  if (!start_node)
    return nullptr;

  // Move forward/backward until non empty visible text node is found.
  for (Node* node = start_node; node; node = Direction::Next(*node)) {
    Node* next_node = Direction::GetVisibleTextNode(*node);
    if (!next_node)
      return nullptr;
    // Filter out nodes without layout object.
    if (next_node->GetLayoutObject() &&
        !PlainText(EphemeralRange::RangeOfContents(*next_node))
             .StripWhiteSpace()
             .IsEmpty())
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
  // Move forward/backward until no next/previous node is available within same
  // |block_ancestor|.
  Node* last_node = nullptr;
  for (Node* node = start_node; node; node = Direction::Next(*node)) {
    node = Direction::GetVisibleTextNode(*node);
    if (node && !node->GetLayoutObject())
      continue;

    // Stop, if crossed block boundaries.
    if (!node || !Direction::IsInSameUninterruptedBlock(*start_node, *node))
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

// For Element-based Position returns the node that its pointing to, otherwise
// returns the container node.
Node* ResolvePositionToNode(const PositionInFlatTree& position) {
  Node* node = position.ComputeContainerNode();
  int offset = position.ComputeOffsetInContainerNode();

  if (node->getNodeType() == Node::kElementNode && node->hasChildren() &&
      node->childNodes()->item(offset)) {
    return node->childNodes()->item(offset);
  }
  return node;
}

}  // namespace

constexpr int kExactTextMaxChars = 300;
constexpr int kNoContextMinChars = 20;
constexpr int kMaxContextWords = 10;
constexpr int kMaxRangeWords = 10;
constexpr int kMaxIterationCountToRecord = 10;
constexpr int kMinWordCount_ = 3;

TextFragmentSelectorGenerator::TextFragmentSelectorGenerator(
    LocalFrame* main_frame)
    : frame_(main_frame) {}

void TextFragmentSelectorGenerator::Generate(const RangeInFlatTree& range,
                                             GenerateCallback callback) {
  DCHECK(callback);
  Reset();
  range_ = MakeGarbageCollected<RangeInFlatTree>(range.StartPosition(),
                                                 range.EndPosition());
  pending_generate_selector_callback_ = std::move(callback);

  StartGeneration();
}

void TextFragmentSelectorGenerator::Reset() {
  if (finder_)
    finder_->Cancel();

  generation_start_time_ = base::DefaultTickClock::GetInstance()->NowTicks();
  state_ = kNotStarted;
  error_.reset();
  step_ = kExact;
  max_available_prefix_ = "";
  max_available_suffix_ = "";
  max_available_range_start_ = "";
  max_available_range_end_ = "";
  num_context_words_ = 0;
  num_range_words_ = 0;
  iteration_ = 0;
  selector_ = nullptr;
  range_ = nullptr;
  pending_generate_selector_callback_.Reset();
}

void TextFragmentSelectorGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(range_);
  visitor->Trace(finder_);
}

void TextFragmentSelectorGenerator::RecordSelectorStateUma() const {
  base::UmaHistogramEnumeration("SharedHighlights.LinkGenerated.StateAtRequest",
                                state_);
}

void TextFragmentSelectorGenerator::DidFindMatch(
    const RangeInFlatTree& match,
    const TextFragmentAnchorMetrics::Match match_metrics,
    bool is_unique) {
  if (is_unique &&
      PlainText(match.ToEphemeralRange()).StripWhiteSpace().length() ==
          PlainText(range_->ToEphemeralRange()).StripWhiteSpace().length()) {
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

void TextFragmentSelectorGenerator::NoMatchFound() {
  state_ = kFailure;
  error_ = LinkGenerationError::kIncorrectSelector;
  ResolveSelectorState();
}

void TextFragmentSelectorGenerator::AdjustSelection() {
  if (!range_)
    return;

  EphemeralRangeInFlatTree ephemeral_range = range_->ToEphemeralRange();
  Node* start_container =
      ephemeral_range.StartPosition().ComputeContainerNode();
  Node* end_container = ephemeral_range.EndPosition().ComputeContainerNode();
  Node* corrected_start =
      ResolvePositionToNode(ephemeral_range.StartPosition());
  int corrected_start_offset =
      (corrected_start->isSameNode(start_container))
          ? ephemeral_range.StartPosition().ComputeOffsetInContainerNode()
          : 0;

  Node* corrected_end = ResolvePositionToNode(ephemeral_range.EndPosition());
  int corrected_end_offset =
      (corrected_end->isSameNode(end_container))
          ? ephemeral_range.EndPosition().ComputeOffsetInContainerNode()
          : 0;

  // If start node has no text or given start position point to the last visible
  // text in its containiner node, use the following visible node for selection
  // start. This has to happen before generation, so that selection is correctly
  // classified as same block or not.
  if (IsLastVisiblePosition(corrected_start, corrected_start_offset)) {
    corrected_start = FirstNonEmptyVisibleTextNode(
        FlatTreeTraversal::NextSkippingChildren(*corrected_start));
    corrected_start_offset = 0;
  } else {
    // if node change was not necessary move start and end positions to
    // contain full words. This is not necessary when node change happened
    // because block limits are also word limits.
    String start_text = corrected_start->textContent();
    start_text.Ensure16Bit();
    corrected_start_offset = FindWordStartBoundary(
        start_text.Characters16(), start_text.length(), corrected_start_offset);
  }

  // If end node has no text or given end position point to the first visible
  // text in its containiner node, use the previous visible node for selection
  // end. This has to happen before generation, so that selection is correctly
  // classified as same block or not.
  if (IsFirstVisiblePosition(corrected_end, corrected_end_offset)) {
    // Here, |Previous()| already skips the children of the given node,
    // because we're doing pre-order traversal.
    corrected_end = BackwardNonEmptyVisibleTextNode(
        FlatTreeTraversal::Previous(*corrected_end));
    if (corrected_end)
      corrected_end_offset = corrected_end->textContent().length();
  } else {
    // if node change was not necessary move start and end positions to
    // contain full words. This is not necessary when node change happened
    // because block limits are also word limits.
    String end_text = corrected_end->textContent();
    end_text.Ensure16Bit();

    // If |selection_end_pos| is at the beginning of a new word then don't
    // search for the word end as it will be the end of the next word, which was
    // not included in the selection.
    if (corrected_end_offset != FindWordStartBoundary(end_text.Characters16(),
                                                      end_text.length(),
                                                      corrected_end_offset)) {
      corrected_end_offset = FindWordEndBoundary(
          end_text.Characters16(), end_text.length(), corrected_end_offset);
    }
  }

  if (corrected_start != start_container ||
      static_cast<int>(corrected_start_offset) !=
          ephemeral_range.StartPosition().ComputeOffsetInContainerNode() ||
      corrected_end != end_container ||
      static_cast<int>(corrected_end_offset) !=
          ephemeral_range.EndPosition().ComputeOffsetInContainerNode()) {
    PositionInFlatTree start(corrected_start, corrected_start_offset);
    PositionInFlatTree end(corrected_end, corrected_end_offset);

    // TODO(bokan): This can sometimes occur from a selection. Avoid crashing
    // from this case but this can come from a seemingly correct range so we
    // should investigate the source of the bug.  https://crbug.com/1216357
    if (start >= end) {
      range_ = nullptr;
      return;
    }

    range_ = MakeGarbageCollected<RangeInFlatTree>(start, end);
  }
}

void TextFragmentSelectorGenerator::StartGeneration() {
  DCHECK(range_);

  range_->StartPosition().GetDocument()->UpdateStyleAndLayout(
      DocumentUpdateReason::kFindInPage);

  // TODO(bokan): This can sometimes occur from a selection. Avoid crashing from
  // this case but this can come from a seemingly correct range so we should
  // investigate the source of the bug.
  // https://crbug.com/1216357
  EphemeralRangeInFlatTree ephemeral_range = range_->ToEphemeralRange();
  if (ephemeral_range.StartPosition() >= ephemeral_range.EndPosition()) {
    state_ = kFailure;
    error_ = LinkGenerationError::kEmptySelection;
    ResolveSelectorState();
    return;
  }

  // Shouldn't continue if selection is empty.
  String selected_text = PlainText(ephemeral_range).StripWhiteSpace();
  if (selected_text.IsEmpty()) {
    state_ = kFailure;
    error_ = LinkGenerationError::kEmptySelection;
    ResolveSelectorState();
    return;
  }

  AdjustSelection();

  // TODO(bokan): This can sometimes occur from a selection. Avoid crashing from
  // this case but this can come from a seemingly correct range so we should
  // investigate the source of the bug.
  // https://crbug.com/1216357
  if (!range_) {
    state_ = kFailure;
    error_ = LinkGenerationError::kEmptySelection;
    ResolveSelectorState();
    return;
  }

  UMA_HISTOGRAM_COUNTS_1000("SharedHighlights.LinkGenerated.SelectionLength",
                            PlainText(range_->ToEphemeralRange()).length());
  state_ = kNeedsNewCandidate;
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
    case kNotStarted:
    case kNeedsNewCandidate:
      NOTREACHED();
      ABSL_FALLTHROUGH_INTENDED;
    case kFailure:
      OnSelectorReady(
          TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
      break;
    case kSuccess:
      OnSelectorReady(*selector_);
      break;
  }
}

void TextFragmentSelectorGenerator::RunTextFinder() {
  DCHECK(selector_);
  iteration_++;
  // |FindMatch| will call |DidFindMatch| indicating if the match was unique.
  finder_ = MakeGarbageCollected<TextFragmentFinder>(
      *this, *selector_, frame_->GetDocument(),
      TextFragmentFinder::FindBufferRunnerType::kAsynchronous);
  finder_->FindMatch();
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
        FlatTreeTraversal::Previous(*prefix_end));

    if (!prefix_end)
      return "";
    prefix_end_offset = prefix_end->textContent().length();
  }

  // The furthest node within same block without crossing block boundaries would
  // be the prefix start.
  Node* prefix_start = LastVisibleTextNodeWithinBlock(prefix_end);
  if (!prefix_start)
    return "";

  auto range_start = Position(prefix_start, 0);
  auto range_end = Position(prefix_end, prefix_end_offset);
  // TODO(gayane): Find test case when this happens, seems related to shadow
  // root. See crbug.com/1220830
  if (range_start >= range_end)
    return "";
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

  // TODO(gayane): Find test case when this happens, seems related to shadow
  // root. See crbug.com/1220830
  if (range_start >= range_end)
    return "";
  return PlainText(EphemeralRange(range_start, range_end)).StripWhiteSpace();
}

void TextFragmentSelectorGenerator::GenerateExactSelector() {
  DCHECK_EQ(kExact, step_);
  DCHECK_EQ(kNeedsNewCandidate, state_);
  EphemeralRangeInFlatTree ephemeral_range = range_->ToEphemeralRange();

  // TODO(bokan): Another case where the range appears to not have valid nodes.
  // Not sure how this happens. https://crbug.com/1216773.
  if (!ephemeral_range.StartPosition().ComputeContainerNode() ||
      !ephemeral_range.EndPosition().ComputeContainerNode()) {
    state_ = kFailure;
    error_ = LinkGenerationError::kEmptySelection;
    return;
  }

  // If not in same block, should use ranges.
  if (!TextFragmentFinder::IsInSameUninterruptedBlock(
          ephemeral_range.StartPosition(), ephemeral_range.EndPosition())) {
    step_ = kRange;
    return;
  }
  String selected_text = PlainText(ephemeral_range).StripWhiteSpace();
  // If too long should use ranges.
  if (selected_text.length() > kExactTextMaxChars) {
    step_ = kRange;
    return;
  }

  selector_ = std::make_unique<TextFragmentSelector>(
      TextFragmentSelector::SelectorType::kExact, selected_text, "", "", "");

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
  if (num_range_words_ > kMaxRangeWords) {
    step_ = kContext;
    return;
  }

  // Initialize range start/end and word min count, if needed.
  if (max_available_range_start_.IsEmpty() &&
      max_available_range_end_.IsEmpty()) {
    EphemeralRangeInFlatTree ephemeral_range = range_->ToEphemeralRange();

    // If selection starts and ends in the same block, then split selected text
    // roughly in the middle.
    if (TextFragmentFinder::IsInSameUninterruptedBlock(
            ephemeral_range.StartPosition(), ephemeral_range.EndPosition())) {
      String selection_text = PlainText(ephemeral_range);
      selection_text.Ensure16Bit();
      int selection_length = selection_text.length();
      int mid_point =
          FindNextWordForward(selection_text.Characters16(),
                              selection_text.length(), selection_length / 2);
      max_available_range_start_ = selection_text.Left(mid_point);

      // If from middle till end of selection there is no word break, then we
      // cannot use it for range end.
      if (mid_point == selection_length) {
        state_ = kFailure;
        error_ = LinkGenerationError::kNoRange;
        return;
      }

      max_available_range_end_ =
          selection_text.Right(selection_text.length() - mid_point - 1);
    } else {
      // If not the same node, then we use first and last block of the selection
      // range.
      max_available_range_start_ =
          GetNextTextBlock(ToPositionInDOMTree(range_->StartPosition()));
      max_available_range_end_ =
          GetPreviousTextBlock(ToPositionInDOMTree(range_->EndPosition()));
    }

    // Use at least 3 words from both sides for more robust link to text.
    num_range_words_ = kMinWordCount_;
  }

  String start =
      GetWordsFromStart(max_available_range_start_, num_range_words_);
  String end = GetWordsFromEnd(max_available_range_end_, num_range_words_);
  num_range_words_++;

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
  if (num_context_words_ == kMaxContextWords) {
    state_ = kFailure;
    error_ = LinkGenerationError::kContextLimitReached;
    return;
  }

  // Try initiating properties necessary for calculating prefix and suffix.
  if (max_available_prefix_.IsEmpty() && max_available_suffix_.IsEmpty()) {
    max_available_prefix_ =
        GetPreviousTextBlock(ToPositionInDOMTree(range_->StartPosition()));
    max_available_suffix_ =
        GetNextTextBlock(ToPositionInDOMTree(range_->EndPosition()));

    // Use at least 3 words from both sides for more robust link to text.
    num_context_words_ = kMinWordCount_;
  }

  if (max_available_prefix_.IsEmpty() && max_available_suffix_.IsEmpty()) {
    state_ = kFailure;
    error_ = LinkGenerationError::kNoContext;
    return;
  }

  String prefix = GetWordsFromEnd(max_available_prefix_, num_context_words_);
  String suffix = GetWordsFromStart(max_available_suffix_, num_context_words_);
  num_context_words_++;

  // Give up if we were unable to get new prefix and suffix.
  if (prefix == selector_->Prefix() && suffix == selector_->Suffix()) {
    state_ = kFailure;
    error_ = LinkGenerationError::kContextExhausted;
    return;
  }
  selector_ = std::make_unique<TextFragmentSelector>(
      selector_->Type(), selector_->Start(), selector_->End(), prefix, suffix);

  state_ = kTestCandidate;
}

void TextFragmentSelectorGenerator::RecordAllMetrics(
    const TextFragmentSelector& selector) {
  UMA_HISTOGRAM_BOOLEAN(
      "SharedHighlights.LinkGenerated",
      selector.Type() != TextFragmentSelector::SelectorType::kInvalid);

  ukm::UkmRecorder* recorder = frame_->GetDocument()->UkmRecorder();
  ukm::SourceId source_id = frame_->GetDocument()->UkmSourceID();

  if (selector.Type() != TextFragmentSelector::SelectorType::kInvalid) {
    UMA_HISTOGRAM_COUNTS_1000("SharedHighlights.LinkGenerated.ParamLength",
                              selector.ToString().length());

    UMA_HISTOGRAM_EXACT_LINEAR("SharedHighlights.LinkGenerated.Iterations",
                               iteration_, kMaxIterationCountToRecord);
    UMA_HISTOGRAM_TIMES("SharedHighlights.LinkGenerated.TimeToGenerate",
                        base::DefaultTickClock::GetInstance()->NowTicks() -
                            generation_start_time_);
    UMA_HISTOGRAM_ENUMERATION(
        "SharedHighlights.LinkGenerated.SelectorParameters",
        TextFragmentAnchorMetrics::GetParametersForSelector(selector));

    shared_highlighting::LogLinkGeneratedSuccessUkmEvent(recorder, source_id);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "SharedHighlights.LinkGenerated.Error.Iterations", iteration_,
        kMaxIterationCountToRecord);
    UMA_HISTOGRAM_TIMES("SharedHighlights.LinkGenerated.Error.TimeToGenerate",
                        base::DefaultTickClock::GetInstance()->NowTicks() -
                            generation_start_time_);

    LinkGenerationError error =
        error_.has_value() ? error_.value() : LinkGenerationError::kUnknown;
    shared_highlighting::LogLinkGenerationErrorReason(error);
    shared_highlighting::LogLinkGeneratedErrorUkmEvent(recorder, source_id,
                                                       error);
  }
}

void TextFragmentSelectorGenerator::OnSelectorReady(
    const TextFragmentSelector& selector) {
  // Check that frame is not deattched and generator is still valid.
  DCHECK(frame_);

  RecordAllMetrics(selector);
  if (pending_generate_selector_callback_) {
    NotifyClientSelectorReady(selector);
  }
}

void TextFragmentSelectorGenerator::NotifyClientSelectorReady(
    const TextFragmentSelector& selector) {
  DCHECK(pending_generate_selector_callback_);
  std::move(pending_generate_selector_callback_).Run(selector, error_);
}

}  // namespace blink
