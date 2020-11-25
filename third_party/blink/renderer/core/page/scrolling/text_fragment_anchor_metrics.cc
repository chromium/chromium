// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

const size_t kMaxTraceEventStringLength = 1000;

}  // namespace

TextFragmentAnchorMetrics::TextFragmentAnchorMetrics(Document* document)
    : document_(document), tick_clock_(base::DefaultTickClock::GetInstance()) {}

void TextFragmentAnchorMetrics::DidCreateAnchor(int selector_count,
                                                int directive_length) {
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchor);
  selector_count_ = selector_count;
  directive_length_ = directive_length;
}

void TextFragmentAnchorMetrics::DidFindMatch(Match match) {
  matches_.push_back(match);
}

void TextFragmentAnchorMetrics::ResetMatchCount() {
  matches_.clear();
}

void TextFragmentAnchorMetrics::DidFindAmbiguousMatch() {
  ambiguous_match_ = true;
}

void TextFragmentAnchorMetrics::ScrollCancelled() {
  scroll_cancelled_ = true;
}

void TextFragmentAnchorMetrics::DidStartSearch() {
  if (search_start_time_.is_null())
    search_start_time_ = tick_clock_->NowTicks();
}

void TextFragmentAnchorMetrics::DidScroll() {
  if (first_scroll_into_view_time_.is_null())
    first_scroll_into_view_time_ = tick_clock_->NowTicks();
}

void TextFragmentAnchorMetrics::DidNonZeroScroll() {
  did_non_zero_scroll_ = true;
}

void TextFragmentAnchorMetrics::DidScrollToTop() {
  if (did_scroll_to_top_)
    return;
  did_scroll_to_top_ = true;

  base::TimeTicks scroll_to_top_time = tick_clock_->NowTicks();

  DCHECK(!first_scroll_into_view_time_.is_null());
  DCHECK(scroll_to_top_time >= first_scroll_into_view_time_);

  std::string uma_prefix = GetPrefixForHistograms();

  base::TimeDelta time_to_scroll_to_top(scroll_to_top_time -
                                        first_scroll_into_view_time_);
  base::UmaHistogramTimes(base::StrCat({uma_prefix, "TimeToScrollToTop"}),
                          time_to_scroll_to_top);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "time_to_scroll_to_top",
                       time_to_scroll_to_top.InMilliseconds());
}

void TextFragmentAnchorMetrics::ReportMetrics() {
#ifndef NDEBUG
  DCHECK(!metrics_reported_);
#endif
  DCHECK(selector_count_);
  DCHECK(matches_.size() <= selector_count_);
  DCHECK(!search_start_time_.is_null());

  if (matches_.size() > 0) {
    UseCounter::Count(document_, WebFeature::kTextFragmentAnchorMatchFound);
  }

  std::string uma_prefix = GetPrefixForHistograms();

  base::UmaHistogramCounts100(base::StrCat({uma_prefix, "SelectorCount"}),
                              selector_count_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "selector_count",
                       selector_count_);

  base::UmaHistogramCounts1000(base::StrCat({uma_prefix, "DirectiveLength"}),
                               directive_length_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "directive_length",
                       directive_length_);

  const int match_rate_percent =
      static_cast<int>(100 * ((matches_.size() + 0.0) / selector_count_));
  base::UmaHistogramPercentage(base::StrCat({uma_prefix, "MatchRate"}),
                               match_rate_percent);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "match_rate",
                       match_rate_percent);

  for (const Match& match : matches_) {
    TRACE_EVENT_INSTANT2(
        "blink", "TextFragmentAnchorMetrics::ReportMetrics",
        TRACE_EVENT_SCOPE_THREAD, "match_found",
        match.text.Utf8().substr(0, kMaxTraceEventStringLength), "match_length",
        match.text.length());

    if (match.selector.Type() == TextFragmentSelector::kExact) {
      base::UmaHistogramCounts1000(
          base::StrCat({uma_prefix, "ExactTextLength"}), match.text.length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "exact_text_length",
                           match.text.length());

      base::UmaHistogramBoolean(base::StrCat({uma_prefix, "ListItemMatch"}),
                                match.is_list_item);
      base::UmaHistogramBoolean(base::StrCat({uma_prefix, "TableCellMatch"}),
                                match.is_table_cell);
    } else if (match.selector.Type() == TextFragmentSelector::kRange) {
      base::UmaHistogramCounts1000(
          base::StrCat({uma_prefix, "RangeMatchLength"}), match.text.length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "range_match_length",
                           match.text.length());

      base::UmaHistogramCounts1000(
          base::StrCat({uma_prefix, "StartTextLength"}),
          match.selector.Start().length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "start_text_length",
                           match.selector.Start().length());

      base::UmaHistogramCounts1000(base::StrCat({uma_prefix, "EndTextLength"}),
                                   match.selector.End().length());
      TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                           TRACE_EVENT_SCOPE_THREAD, "end_text_length",
                           match.selector.End().length());

      // We only record ListItemMatch and TableCellMatch for exact matches
      DCHECK(!match.is_list_item && !match.is_table_cell);
    }

    base::UmaHistogramEnumeration(base::StrCat({uma_prefix, "Parameters"}),
                                  GetParametersForSelector(match.selector));
  }

  base::UmaHistogramBoolean(base::StrCat({uma_prefix, "AmbiguousMatch"}),
                            ambiguous_match_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "ambiguous_match",
                       ambiguous_match_);

  base::UmaHistogramBoolean(base::StrCat({uma_prefix, "ScrollCancelled"}),
                            scroll_cancelled_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "scroll_cancelled",
                       scroll_cancelled_);

  if (!first_scroll_into_view_time_.is_null()) {
    DCHECK(first_scroll_into_view_time_ >= search_start_time_);

    base::UmaHistogramBoolean(base::StrCat({uma_prefix, "DidScrollIntoView"}),
                              did_non_zero_scroll_);
    TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                         TRACE_EVENT_SCOPE_THREAD, "did_scroll_into_view",
                         did_non_zero_scroll_);

    base::TimeDelta time_to_scroll_into_view(first_scroll_into_view_time_ -
                                             search_start_time_);
    base::UmaHistogramTimes(base::StrCat({uma_prefix, "TimeToScrollIntoView"}),
                            time_to_scroll_into_view);
    TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                         TRACE_EVENT_SCOPE_THREAD, "time_to_scroll_into_view",
                         time_to_scroll_into_view.InMilliseconds());
  }

  base::UmaHistogramEnumeration("TextFragmentAnchor.LinkOpenSource",
                                has_search_engine_source_
                                    ? TextFragmentLinkOpenSource::kSearchEngine
                                    : TextFragmentLinkOpenSource::kUnknown);
#ifndef NDEBUG
  metrics_reported_ = true;
#endif
}

void TextFragmentAnchorMetrics::Dismissed() {
  // We report Dismissed separately from ReportMetrics as it may or may not
  // get called in the lifetime of the TextFragmentAnchor.
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchorTapToDismiss);
  TRACE_EVENT_INSTANT0("blink", "TextFragmentAnchorMetrics::Dismissed",
                       TRACE_EVENT_SCOPE_THREAD);
}

void TextFragmentAnchorMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

TextFragmentAnchorMetrics::TextFragmentAnchorParameters
TextFragmentAnchorMetrics::GetParametersForSelector(
    const TextFragmentSelector& selector) {
  TextFragmentAnchorParameters parameters =
      TextFragmentAnchorParameters::kUnknown;

  if (selector.Type() == TextFragmentSelector::SelectorType::kExact) {
    if (selector.Prefix().length() && selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithContext;
    else if (selector.Prefix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithPrefix;
    else if (selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kExactTextWithSuffix;
    else
      parameters = TextFragmentAnchorParameters::kExactText;
  } else if (selector.Type() == TextFragmentSelector::SelectorType::kRange) {
    if (selector.Prefix().length() && selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithContext;
    else if (selector.Prefix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithPrefix;
    else if (selector.Suffix().length())
      parameters = TextFragmentAnchorParameters::kTextRangeWithSuffix;
    else
      parameters = TextFragmentAnchorParameters::kTextRange;
  }

  return parameters;
}

void TextFragmentAnchorMetrics::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void TextFragmentAnchorMetrics::SetSearchEngineSource(
    bool has_search_engine_source) {
  has_search_engine_source_ = has_search_engine_source;
}

std::string TextFragmentAnchorMetrics::GetPrefixForHistograms() const {
  std::string source = has_search_engine_source_ ? "SearchEngine" : "Unknown";
  std::string uma_prefix = base::StrCat({"TextFragmentAnchor.", source, "."});
  return uma_prefix;
}

}  // namespace blink
