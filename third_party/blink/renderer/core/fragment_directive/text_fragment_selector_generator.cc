// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/range_in_flat_tree.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_finder.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/text/text_boundaries.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

using LinkGenerationError = shared_highlighting::LinkGenerationError;
using LinkGenerationStatus = shared_highlighting::LinkGenerationStatus;

namespace blink {

namespace {

// Returns true if text from beginning of |node| until |pos_offset| can be
// considered empty. Otherwise, return false.
bool IsFirstVisiblePosition(Node* node, unsigned pos_offset) {
  auto range_start = PositionInFlatTree::FirstPositionInNode(*node);
  auto range_end = PositionInFlatTree(node, pos_offset);
  return node->getNodeType() == Node::kElementNode || pos_offset == 0 ||
         PlainText(EphemeralRangeInFlatTree(range_start, range_end))
                 .LengthWithStrippedWhiteSpace() == 0;
}

// Returns true if text from |pos_offset| until end of |node| can be considered
// empty. Otherwise, return false.
bool IsLastVisiblePosition(Node* node, unsigned pos_offset) {
  auto range_start = PositionInFlatTree(node, pos_offset);
  auto range_end = PositionInFlatTree::LastPositionInNode(*node);
  return node->getNodeType() == Node::kElementNode ||
         pos_offset == node->textContent().length() ||
         PlainText(EphemeralRangeInFlatTree(range_start, range_end))
                 .LengthWithStrippedWhiteSpace() == 0;
}

struct ForwardDirection {
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
    if (!next_node)
      return nullptr;
    // Filter out nodes without layout object.
    if (next_node->GetLayoutObject() &&
        PlainText(EphemeralRange::RangeOfContents(*next_node))
                .LengthWithStrippedWhiteSpace() > 0) {
      return next_node;
    }
    node = next_node;
  }
  return nullptr;
}

// Returns the next/previous visible node to |start_node|.
Node* FirstNonEmptyVisibleTextNode(Node* start_node) {
  return NextNonEmptyVisibleTextNode<ForwardDirection>(start_node);
}

Node* BackwardNonEmptyVisibleTextNode(Node* start_node) {
  return NextNonEmptyVisibleTextNode<BackwardDirection>(start_node);
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
constexpr int kMinWordCount = 3;

std::optional<int> g_exactTextMaxCharsOverride;

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
  if (finder_) {
    finder_->Cancel();
    finder_.Clear();
  }

  generation_start_time_ = base::DefaultTickClock::GetInstance()->NowTicks();
  state_ = kNotStarted;
  error_ = LinkGenerationError::kNone;
  step_ = kExact;
  prefix_iterator_ = nullptr;
  suffix_iterator_ = nullptr;
  range_start_iterator_ = nullptr;
  range_end_iterator_ = nullptr;
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
  visitor->Trace(prefix_iterator_);
  visitor->Trace(suffix_iterator_);
  visitor->Trace(range_start_iterator_);
  visitor->Trace(range_end_iterator_);
}

void TextFragmentSelectorGenerator::RecordSelectorStateUma() const {
  base::UmaHistogramEnumeration("SharedHighlights.LinkGenerated.StateAtRequest",
                                state_);
}

String TextFragmentSelectorGenerator::GetSelectorTargetText() const {
  if (!range_)
    return g_empty_string;

  return PlainText(range_->ToEphemeralRange()).StripWhiteSpace();
}

void TextFragmentSelectorGenerator::DidFindMatch(const RangeInFlatTree& match,
                                                 bool is_unique) {
  finder_.Clear();

  if (did_find_match_callback_for_testing_)
    std::move(did_find_match_callback_for_testing_).Run(is_unique);

  if (is_unique &&
      PlainText(match.ToEphemeralRange()).LengthWithStrippedWhiteSpace() ==
          PlainText(range_->ToEphemeralRange())
              .LengthWithStrippedWhiteSpace()) {
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
  finder_.Clear();

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
    corrected_start_offset =
        FindWordStartBoundary(start_text.Span16(), corrected_start_offset);
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
    if (corrected_end_offset !=
        FindWordStartBoundary(end_text.Span16(), corrected_end_offset)) {
      corrected_end_offset =
          FindWordEndBoundary(end_text.Span16(), corrected_end_offset);
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
  if (PlainText(ephemeral_range).LengthWithStrippedWhiteSpace() == 0) {
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
      NOTREACHED_IN_MIGRATION();
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

PositionInFlatTree TextFragmentSelectorGenerator::GetPreviousTextEndPosition(
    const PositionInFlatTree& position) {
  PositionInFlatTree search_end_position =
      PositionInFlatTree::FirstPositionInNode(
          *frame_->GetDocument()->documentElement()->firstChild());
  PositionInFlatTree previous_text_position =
      TextFragmentFinder::PreviousTextPosition(position, search_end_position);
  if (previous_text_position == search_end_position) {
    return PositionInFlatTree();
  }
  return previous_text_position;
}

PositionInFlatTree TextFragmentSelectorGenerator::GetNextTextStartPosition(
    const PositionInFlatTree& position) {
  PositionInFlatTree search_end_position =
      PositionInFlatTree::LastPositionInNode(
          *frame_->GetDocument()->documentElement()->lastChild());
  PositionInFlatTree next_text_position =
      TextFragmentFinder::NextTextPosition(position, search_end_position);

  if (next_text_position == search_end_position) {
    return PositionInFlatTree();
  }
  return next_text_position;
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
  if (selected_text.length() > GetExactTextMaxChars()) {
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

  int num_words_to_add = 1;

  // Determine length of target string for verifictaion.
  unsigned target_length = PlainText(range_->ToEphemeralRange()).length();

  // Initialize range start/end and word min count, if needed.
  if (!range_start_iterator_ && !range_end_iterator_) {
    PositionInFlatTree range_start_position =
        GetNextTextStartPosition(range_->StartPosition());
    PositionInFlatTree range_end_position =
        GetPreviousTextEndPosition(range_->EndPosition());

    if (range_start_position.IsNull() || range_end_position.IsNull()) {
      state_ = kFailure;
      error_ = LinkGenerationError::kNoRange;
      return;
    }

    range_start_iterator_ = MakeGarbageCollected<ForwardSameBlockWordIterator>(
        range_start_position);
    range_end_iterator_ =
        MakeGarbageCollected<BackwardSameBlockWordIterator>(range_end_position);

    // Use at least 3 words from both sides for more robust link to text unless
    // the selected text is shorter than 6 words.
    if (TextFragmentFinder::IsInSameUninterruptedBlock(range_start_position,
                                                       range_end_position)) {
      num_words_to_add = 0;
      auto* range_start_counter =
          MakeGarbageCollected<ForwardSameBlockWordIterator>(
              range_start_position);
      // TODO(crbug.com/1302719) ForwardSameBlockWordIterator Should be made to
      // return the current posision in a form that is comparable against
      // range_end_position directly.

      while (num_words_to_add < kMinWordCount * 2 &&
             range_start_counter->AdvanceNextWord() &&
             range_start_counter->TextFromStart().length() <= target_length) {
        num_words_to_add++;
      }
      num_words_to_add = num_words_to_add / 2;
      if (num_words_to_add == 0) {
        // If there is only one word found in the range selection explicitly set
        // exact selector to avoid round tripping.
        EphemeralRangeInFlatTree ephemeral_range = range_->ToEphemeralRange();
        String selected_text = PlainText(ephemeral_range).StripWhiteSpace();
        step_ = kExact;
        state_ = kTestCandidate;
        selector_ = std::make_unique<TextFragmentSelector>(
            TextFragmentSelector::SelectorType::kExact, selected_text, "", "",
            "");
        return;
      }
    } else {
      // If the the start and end are in different blocks overlaps dont need to
      // be prevented as the number of words will limited by the block
      // boundaries.
      num_words_to_add = kMinWordCount;
    }
  }

  if (!range_start_iterator_ || !range_end_iterator_) {
    state_ = kFailure;
    error_ = LinkGenerationError::kNoRange;
    return;
  }

  for (int i = 0; i < num_words_to_add; i++) {
    if (range_start_iterator_)
      range_start_iterator_->AdvanceNextWord();

    if (range_end_iterator_)
      range_end_iterator_->AdvanceNextWord();

    num_range_words_++;
  }

  String start =
      range_start_iterator_ ? range_start_iterator_->TextFromStart() : "";
  String end = range_end_iterator_ ? range_end_iterator_->TextFromStart() : "";

  if (start.length() + end.length() > target_length) {
    if (!selector_) {
      state_ = kFailure;
      error_ = LinkGenerationError::kNoRange;
      return;
    }

    // If start and end overlap but its not the first attempt, then proceed with
    // adding context.
    step_ = kContext;
    return;
  }

  if (selector_ && start == selector_->Start() && end == selector_->End()) {
    // If the start and end didn't change, it means we
    // exhausted the selected text and should try adding context.
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
  if (num_context_words_ >= kMaxContextWords) {
    state_ = kFailure;
    error_ = LinkGenerationError::kContextLimitReached;
    return;
  }

  int num_words_to_add = 1;
  // Try initiating properties necessary for calculating prefix and suffix.
  if (!suffix_iterator_ && !prefix_iterator_) {
    PositionInFlatTree suffix_start_position =
        GetNextTextStartPosition(range_->EndPosition());
    PositionInFlatTree prefix_end_position =
        GetPreviousTextEndPosition(range_->StartPosition());

    if (suffix_start_position.IsNotNull()) {
      suffix_iterator_ = MakeGarbageCollected<ForwardSameBlockWordIterator>(
          suffix_start_position);
    }

    if (prefix_end_position.IsNotNull()) {
      prefix_iterator_ = MakeGarbageCollected<BackwardSameBlockWordIterator>(
          prefix_end_position);
    }

    // Use at least 3 words from both sides for more robust link to text.
    num_words_to_add = kMinWordCount;
  }

  if (!suffix_iterator_ && !prefix_iterator_) {
    state_ = kFailure;
    error_ = LinkGenerationError::kNoContext;
    return;
  }

  for (int i = 0; i < num_words_to_add; i++) {
    if (suffix_iterator_)
      suffix_iterator_->AdvanceNextWord();

    if (prefix_iterator_)
      prefix_iterator_->AdvanceNextWord();

    num_context_words_++;
  }

  String prefix = prefix_iterator_ ? prefix_iterator_->TextFromStart() : "";
  String suffix = suffix_iterator_ ? suffix_iterator_->TextFromStart() : "";

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
  LinkGenerationStatus status =
      selector.Type() == TextFragmentSelector::SelectorType::kInvalid
          ? LinkGenerationStatus::kFailure
          : LinkGenerationStatus::kSuccess;
  shared_highlighting::LogLinkGenerationStatus(status);

  ukm::UkmRecorder* recorder = frame_->GetDocument()->UkmRecorder();
  ukm::SourceId source_id = frame_->GetDocument()->UkmSourceID();

  if (selector.Type() != TextFragmentSelector::SelectorType::kInvalid) {
    UMA_HISTOGRAM_TIMES("SharedHighlights.LinkGenerated.TimeToGenerate",
                        base::DefaultTickClock::GetInstance()->NowTicks() -
                            generation_start_time_);

    shared_highlighting::LogLinkGeneratedSuccessUkmEvent(recorder, source_id);
  } else {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "SharedHighlights.LinkGenerated.Error.Iterations", iteration_,
        kMaxIterationCountToRecord);
    UMA_HISTOGRAM_TIMES("SharedHighlights.LinkGenerated.Error.TimeToGenerate",
                        base::DefaultTickClock::GetInstance()->NowTicks() -
                            generation_start_time_);
    if (error_ == LinkGenerationError::kNone)
      error_ = LinkGenerationError::kUnknown;
    shared_highlighting::LogLinkGenerationErrorReason(error_);
    shared_highlighting::LogLinkGeneratedErrorUkmEvent(recorder, source_id,
                                                       error_);
  }
}

void TextFragmentSelectorGenerator::OnSelectorReady(
    const TextFragmentSelector& selector) {
  // Check that frame is not deattched and generator is still valid.
  DCHECK(frame_);

  RecordAllMetrics(selector);
  if (pending_generate_selector_callback_) {
    std::move(pending_generate_selector_callback_).Run(selector, error_);
  }
}

// static
void TextFragmentSelectorGenerator::OverrideExactTextMaxCharsForTesting(
    int value) {
  if (value < 0)
    g_exactTextMaxCharsOverride.reset();
  else
    g_exactTextMaxCharsOverride = value;
}

unsigned TextFragmentSelectorGenerator::GetExactTextMaxChars() {
  if (g_exactTextMaxCharsOverride)
    return g_exactTextMaxCharsOverride.value();
  else
    return kExactTextMaxChars;
}

}  // namespace blink
