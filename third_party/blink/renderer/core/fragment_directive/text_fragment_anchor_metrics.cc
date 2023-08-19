// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

TextFragmentAnchorMetrics::TextFragmentAnchorMetrics(Document* document)
    : document_(document), tick_clock_(base::DefaultTickClock::GetInstance()) {}

void TextFragmentAnchorMetrics::DidCreateAnchor(int selector_count) {
  UseCounter::Count(document_, WebFeature::kTextFragmentAnchor);
  selector_count_ = selector_count;
  CHECK(search_start_time_.is_null());
  search_start_time_ = tick_clock_->NowTicks();
}

void TextFragmentAnchorMetrics::DidFindMatch() {
  ++matches_count_;
}

void TextFragmentAnchorMetrics::DidFindAmbiguousMatch() {
  ambiguous_match_ = true;
}

void TextFragmentAnchorMetrics::DidInvokeScrollIntoView() {
  if (first_scroll_into_view_time_.is_null())
    first_scroll_into_view_time_ = tick_clock_->NowTicks();
}

void TextFragmentAnchorMetrics::ReportMetrics() {
#ifndef NDEBUG
  DCHECK(!metrics_reported_);
#endif
  DCHECK_GT(selector_count_, 0);
  DCHECK_GE(matches_count_, 0);
  DCHECK_LE(matches_count_, selector_count_);
  DCHECK(!search_start_time_.is_null());

  if (matches_count_ > 0) {
    UseCounter::Count(document_, WebFeature::kTextFragmentAnchorMatchFound);
  }

  shared_highlighting::LogLinkOpenedUkmEvent(
      document_->UkmRecorder(), document_->UkmSourceID(),
      GURL(document_->referrer().Utf8()),
      /*success=*/matches_count_ == selector_count_);

  std::string uma_prefix = GetPrefixForHistograms();

  const int match_rate_percent =
      base::ClampFloor((100.0 * matches_count_) / selector_count_);
  base::UmaHistogramPercentage(base::StrCat({uma_prefix, "MatchRate"}),
                               match_rate_percent);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "match_rate",
                       match_rate_percent);

  base::UmaHistogramBoolean(base::StrCat({uma_prefix, "AmbiguousMatch"}),
                            ambiguous_match_);
  TRACE_EVENT_INSTANT1("blink", "TextFragmentAnchorMetrics::ReportMetrics",
                       TRACE_EVENT_SCOPE_THREAD, "ambiguous_match",
                       ambiguous_match_);

  if (!first_scroll_into_view_time_.is_null()) {
    DCHECK(first_scroll_into_view_time_ >= search_start_time_);

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
