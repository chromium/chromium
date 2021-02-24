// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer_runner.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_iterator.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;

// This is a helper class to TextFragmentAnchor. It's responsible for actually
// performing the text search for the anchor and returning the results to the
// anchor.
class CORE_EXPORT TextFragmentFinder final
    : public GarbageCollected<TextFragmentFinder> {
 public:
  class Client {
   public:
    virtual void DidFindMatch(
        const EphemeralRangeInFlatTree& range,
        const TextFragmentAnchorMetrics::Match match_metrics,
        bool is_unique) = 0;
    virtual void NoMatchFound() = 0;
  };

  // Used for tracking what should be the next match stage.
  enum FindBufferRunnerType { kAsynchronous, kSynchronous };
  // Returns true if start and end positions are in the same block and there are
  // no other blocks between them. Otherwise, returns false.
  static bool IsInSameUninterruptedBlock(const PositionInFlatTree& start,
                                         const PositionInFlatTree& end);

  // Client must outlive the finder.
  TextFragmentFinder(Client& client,
                     const TextFragmentSelector& selector,
                     Document* document,
                     FindBufferRunnerType runner_type);
  ~TextFragmentFinder() = default;

  // Begins searching in the given top-level document.
  void FindMatch();

  void Trace(Visitor*) const;

 private:
  // Used for tracking what should be the next match stage.
  enum SelectorMatchStep {
    kMatchPrefix,
    kMatchTextStart,
    kMatchTextEnd,
    kMatchSuffix
  };

  void FindMatchFromPosition(PositionInFlatTree search_start);

  void OnFindMatchInRangeComplete(String search_text,
                                  Range* range,
                                  bool word_start_bounded,
                                  bool word_end_bounded,
                                  const EphemeralRangeInFlatTree& match);

  void FindMatchInRange(String search_text,
                        Range* range,
                        bool word_start_bounded,
                        bool word_end_bounded);

  void FindPrefix();
  void FindTextStart();
  void FindTextEnd();
  void FindSuffix();

  void OnPrefixMatchComplete(EphemeralRangeInFlatTree match);
  void OnTextStartMatchComplete(EphemeralRangeInFlatTree match);
  void OnTextEndMatchComplete(EphemeralRangeInFlatTree match);
  void OnSuffixMatchComplete(EphemeralRangeInFlatTree match);

  void GoToStep(SelectorMatchStep step);

  void OnMatchComplete();

  void SetPotentialMatch(EphemeralRangeInFlatTree range);
  void SetPrefixMatch(EphemeralRangeInFlatTree range);

  Client& client_;
  const TextFragmentSelector selector_;
  Member<Document> document_;
  SelectorMatchStep step_ = kMatchPrefix;

  // Start positions for the next text end |FindTask|, this is separate as the
  // search for end might move the position, which should be discarded.
  PositionInFlatTree range_end_search_start_;

  // Successful match after |FindMatchFromPosition| first run.
  Member<Range> first_match_;
  // Current match after |FindMatchFromPosition| run. See
  // https://wicg.github.io/scroll-to-text-fragment/#ref-for-range-collapsed:~:text=Let-,potentialMatch
  Member<Range> potential_match_;
  // Used for text start match, the presence will change how the text start is
  // matched. See
  // https://wicg.github.io/scroll-to-text-fragment/#ref-for-range-collapsed:~:text=Let-,prefixMatch
  Member<Range> prefix_match_;
  // Range for the current match task, including |prefix_match_|,
  // |potential_match_| and |suffix_match|. See
  // https://wicg.github.io/scroll-to-text-fragment/#ref-for-range-collapsed:~:text=Let-,searchRange
  Member<Range> search_range_;
  // Range used for search for |potential_match_|.
  // https://wicg.github.io/scroll-to-text-fragment/#ref-for-range-collapsed:~:text=Let-,matchRange
  Member<Range> match_range_;
  // Used for running FindBuffer tasks.
  Member<FindBufferRunner> find_buffer_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_FINDER_H_
