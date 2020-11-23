// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

namespace blink {

// Helper class for TextFragmentAnchor that provides hooks for tracking and
// reporting usage and performance metrics to UMA.
class CORE_EXPORT TextFragmentAnchorMetrics final
    : public GarbageCollected<TextFragmentAnchorMetrics> {
 public:
  struct Match {
    explicit Match(TextFragmentSelector text_fragment_selector)
        : selector(text_fragment_selector) {}

    String text;
    TextFragmentSelector selector;
    bool is_list_item = false;
    bool is_table_cell = false;
  };

  // An enum to indicate which parameters were specified in the text fragment.
  enum class TextFragmentAnchorParameters {
    kUnknown = 0,
    kExactText = 1,
    kExactTextWithPrefix = 2,
    kExactTextWithSuffix = 3,
    kExactTextWithContext = 4,
    kTextRange = 5,
    kTextRangeWithPrefix = 6,
    kTextRangeWithSuffix = 7,
    kTextRangeWithContext = 8,
    kMaxValue = kTextRangeWithContext,
  };

  // Update corresponding |TextFragmentLinkOpenSource| in enums.xml.
  enum class TextFragmentLinkOpenSource {
    kUnknown,
    kSearchEngine,

    kMaxValue = kSearchEngine,
  };

  explicit TextFragmentAnchorMetrics(Document* document);

  static TextFragmentAnchorParameters GetParametersForSelector(
      const TextFragmentSelector& selector);

  void DidCreateAnchor(int selector_count, int directive_length);

  void DidFindMatch(Match match);
  void ResetMatchCount();

  void DidFindAmbiguousMatch();

  void ScrollCancelled();

  void DidStartSearch();

  void DidScroll();

  void DidNonZeroScroll();

  void DidScrollToTop();

  void ReportMetrics();

  void Dismissed();

  void SetTickClockForTesting(const base::TickClock* tick_clock);

  void SetSearchEngineSource(bool has_search_engine_source);

  void Trace(Visitor*) const;

 private:
  std::string GetPrefixForHistograms() const;

  Member<Document> document_;

#ifndef NDEBUG
  bool metrics_reported_ = false;
#endif

  wtf_size_t selector_count_ = 0;
  wtf_size_t directive_length_ = 0;
  Vector<Match> matches_;
  bool ambiguous_match_ = false;
  bool scroll_cancelled_ = false;
  base::TimeTicks search_start_time_;
  base::TimeTicks first_scroll_into_view_time_;
  bool did_non_zero_scroll_ = false;
  bool did_scroll_to_top_ = false;
  bool has_search_engine_source_ = false;

  const base::TickClock* tick_clock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_TEXT_FRAGMENT_ANCHOR_METRICS_H_
