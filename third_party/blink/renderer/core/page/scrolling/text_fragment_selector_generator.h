// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_

#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class LocalFrame;
class TextFragmentSelector;

// TextFragmentSelectorGenerator is used to generate a text directive selector
// string, given a range of DOM in a document. The "selector string" is the
// portion of a "scroll-to-text" URL that follows `#:~:text=`. For more
// details, see:
// https://github.com/WICG/scroll-to-text-fragment#proposed-solution.
//
// TextFragmentSelectorGenerator works by starting with a candidate selector
// and repeatedly trying it against the page content to ensure the correct and
// unique match. While we don't have a unique match, we repeatedly adding
// context/range to the selector until the correct match is uniquely identified
// or no new context/range can be added.
class CORE_EXPORT TextFragmentSelectorGenerator final
    : public GarbageCollected<TextFragmentSelectorGenerator>,
      public TextFragmentFinder::Client {
  using RequestSelectorCallback = base::OnceCallback<void(const WTF::String&)>;

 public:
  explicit TextFragmentSelectorGenerator(LocalFrame* main_frame);

  // Sets range for which a selector will be generated when RequestSelector()
  // is called.
  void UpdateSelection(const EphemeralRangeInFlatTree& selection_range);

  // Requests selector for current selection range specified in
  // UpdateSelection. Will be generated asynchronously and returned as a string
  // in the callback. The returned string is the result of
  // TextFragmentSelector::ToString(), i.e. the part that follows "#:~:text="
  // in the URL.
  void RequestSelector(RequestSelectorCallback callback);

  // Resets generator state to initial values and cancels any existing async
  // tasks.
  void Reset();

  // Wrappers for tests.
  String GetPreviousTextBlockForTesting(const Position& position) {
    return GetPreviousTextBlock(position);
  }
  String GetNextTextBlockForTesting(const Position& position) {
    return GetNextTextBlock(position);
  }
  void SetCallbackForTesting(RequestSelectorCallback callback) {
    pending_generate_selector_callback_ = std::move(callback);
  }

  // Called when the frame is detached. Releases members if necessary.
  void ClearSelection();

  // Called when the document is detached.
  void Detach();

  void Trace(Visitor*) const;

  LocalFrame* GetFrame() { return frame_; }

 private:
  // Used for determining the next step of selector generation.
  enum GenerationStep { kExact, kRange, kContext };

  // Used for determining the current state of |selector_|.
  enum SelectorState {
    // Sreach for candidate selector didn't start.
    kNotStarted,

    // Candidate selector should be generated or extended.
    kNeedsNewCandidate,

    // Candidate selector generation was successful and selector is ready to be
    // tested for uniqueness and accuracy by running against the page's content.
    kTestCandidate,

    // Candidate selector generation was unsuccessful. No further attempts are
    // necessary.
    kFailure,

    // Selector is found. No further attempts are necessary.
    kSuccess,

    kMaxValue = kSuccess
  };

  // TextFragmentFinder::Client interface
  void DidFindMatch(const EphemeralRangeInFlatTree& match,
                    const TextFragmentAnchorMetrics::Match match_metrics,
                    bool is_unique) override;
  void NoMatchFound() override;

  // Adjust the selection start/end to a valid position. That includes skipping
  // non text start/end nodes and extending selection from start and end to
  // contain full words.
  void AdjustSelection();

  // Generates selector for current selection.
  void GenerateSelector();

  void GenerateSelectorCandidate();

  void ResolveSelectorState();
  void RunTextFinder();

  // Returns max text preceding given position that doesn't cross block
  // boundaries.
  String GetPreviousTextBlock(const Position& position);

  // Returns max text following given position that doesn't cross block
  // boundaries.
  String GetNextTextBlock(const Position& position);

  void GenerateExactSelector();
  void ExtendRangeSelector();
  void ExtendContext();

  void RecordAllMetrics(const TextFragmentSelector& selector);
  void RecordPreemptiveGenerationMetrics(const TextFragmentSelector& selector);

  // Called when selector generation is complete.
  void OnSelectorReady(const TextFragmentSelector& selector);

  // Called to notify clients of the result of |RequestSelector|.
  void NotifyClientSelectorReady(const TextFragmentSelector& selector);

  Member<LocalFrame> frame_;
  Member<Range> selection_range_;
  std::unique_ptr<TextFragmentSelector> selector_;

  RequestSelectorCallback pending_generate_selector_callback_;

  GenerationStep step_ = kExact;
  SelectorState state_ = kNeedsNewCandidate;

  // Used when preemptive link generation is enabled to report
  // whether |RequestSelector| was called before or after selector was ready.
  absl::optional<bool> selector_requested_before_ready_;

  absl::optional<shared_highlighting::LinkGenerationError> error_;

  // Fields used for keeping track of context.

  // Strings available for gradually forming prefix and suffix.
  String max_available_prefix_;
  String max_available_suffix_;

  String max_available_range_start_;
  String max_available_range_end_;

  // Indicates a number of words used from |max_available_prefix_| and
  // |max_available_suffix_| for the current |selector_|.
  int num_context_words_ = 0;

  int num_range_words_ = 0;

  int iteration_ = 0;
  base::TimeTicks generation_start_time_;

  Member<TextFragmentFinder> finder_;

  DISALLOW_COPY_AND_ASSIGN(TextFragmentSelectorGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_SELECTOR_GENERATOR_H_
