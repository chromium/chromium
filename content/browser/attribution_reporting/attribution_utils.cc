// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/filters.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/common_source_info.h"

namespace content {

namespace {

constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

base::span<const base::TimeDelta> EarlyDeadlines(
    AttributionSourceType source_type) {
  static constexpr base::TimeDelta kEarlyDeadlinesNavigation[] = {
      base::Days(2),
      base::Days(7),
  };

  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return kEarlyDeadlinesNavigation;
    case AttributionSourceType::kEvent:
      return base::span<const base::TimeDelta>();
  }
}

base::TimeDelta ExpiryDeadline(const CommonSourceInfo& source) {
  return source.event_report_window_time() - source.source_time();
}

base::Time ReportTimeFromDeadline(base::Time source_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline.is_zero());
  return source_time + deadline + kWindowDeadlineOffset;
}

}  // namespace

base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time trigger_time) {
  base::TimeDelta expiry_deadline = ExpiryDeadline(source);

  // After the initial impression, a schedule of reporting windows and deadlines
  // associated with that impression begins. The time between impression time
  // and impression expiry is split into multiple reporting windows. At the end
  // of each window, the browser will send all scheduled reports for that
  // impression.
  //
  // Each reporting window has a deadline and only conversions registered before
  // that deadline are sent in that window. Each deadline is one hour prior to
  // the window report time. The deadlines relative to impression time are <2
  // days minus 1 hour, 7 days minus 1 hour, impression expiry>. The impression
  // expiry window is only used for conversions that occur after the 7 day
  // deadline. For example, a conversion which happens one hour after an
  // impression with an expiry of two hours, is still reported in the 2 day
  // window.
  //
  // Note that only navigation (not event) sources have early reporting
  // deadlines.
  base::TimeDelta deadline_to_use = expiry_deadline;

  // Given a conversion that happened at `trigger_time`, find the first
  // applicable reporting window this conversion should be reported at.
  for (base::TimeDelta early_deadline : EarlyDeadlines(source.source_type())) {
    // If this window is valid for the conversion, use it.
    // |trigger_time| is roughly ~now.
    if (source.source_time() + early_deadline >= trigger_time &&
        early_deadline < deadline_to_use) {
      deadline_to_use = early_deadline;
      break;
    }
  }

  return ReportTimeFromDeadline(source.source_time(), deadline_to_use);
}

int NumReportWindows(AttributionSourceType source_type) {
  // Add 1 for the expiry deadline.
  return 1 + EarlyDeadlines(source_type).size();
}

base::Time ReportTimeAtWindow(const CommonSourceInfo& source,
                              int window_index) {
  DCHECK_GE(window_index, 0);
  DCHECK_LT(window_index, NumReportWindows(source.source_type()));

  base::span<const base::TimeDelta> early_deadlines =
      EarlyDeadlines(source.source_type());

  base::TimeDelta deadline =
      static_cast<size_t>(window_index) < early_deadlines.size()
          ? early_deadlines[window_index]
          : ExpiryDeadline(source);

  return ReportTimeFromDeadline(source.source_time(), deadline);
}

std::string SerializeAttributionJson(base::ValueView body, bool pretty_print) {
  int options = pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0;

  std::string output_json;
  bool success =
      base::JSONWriter::WriteWithOptions(body, options, &output_json);
  DCHECK(success);
  return output_json;
}

bool AttributionFilterDataMatch(const attribution_reporting::FilterData& source,
                                AttributionSourceType source_type,
                                const attribution_reporting::Filters& trigger,
                                bool negated) {
  // A filter is considered matched if the filter key is only present either on
  // the source or trigger, or the intersection of the filter values is
  // non-empty.
  // Returns true if all the filters matched.
  //
  // If the filters are negated, the behavior should be that every single filter
  // key does not match between the two (negating the function result is not
  // sufficient by the API definition).
  return base::ranges::all_of(
      trigger.filter_values(), [&](const auto& trigger_filter) {
        if (trigger_filter.first ==
            attribution_reporting::FilterData::kSourceTypeFilterKey) {
          bool has_intersection = base::ranges::any_of(
              trigger_filter.second, [&](const std::string& value) {
                return value == AttributionSourceTypeToString(source_type);
              });

          return negated != has_intersection;
        }

        auto source_filter = source.filter_values().find(trigger_filter.first);
        if (source_filter == source.filter_values().end())
          return true;

        // Desired behavior is to treat any empty set of values as a single
        // unique value itself. This means:
        //  - x:[] match x:[] is false when negated, and true otherwise.
        //  - x:[1,2,3] match x:[] is true when negated, and false otherwise.
        if (trigger_filter.second.empty())
          return negated != source_filter->second.empty();

        bool has_intersection = base::ranges::any_of(
            trigger_filter.second, [&](const std::string& value) {
              return base::Contains(source_filter->second, value);
            });
        // Negating filters are considered matched if the intersection of the
        // filter values is empty.
        return negated != has_intersection;
      });
}

bool AttributionFiltersMatch(
    const attribution_reporting::FilterData& source_filter_data,
    AttributionSourceType source_type,
    const attribution_reporting::Filters& trigger_filters,
    const attribution_reporting::Filters& trigger_not_filters) {
  return AttributionFilterDataMatch(source_filter_data, source_type,
                                    trigger_filters) &&
         AttributionFilterDataMatch(source_filter_data, source_type,
                                    trigger_not_filters,
                                    /*negated=*/true);
}

}  // namespace content
