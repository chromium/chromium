// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_url_generator.h"

#include "base/strings/string_number_conversions.h"
#include "google_apis/common/time_util.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace google_apis {

namespace calendar {

namespace {

// Hard coded URLs for communication with a google calendar server.
constexpr char kCalendarV3EventsUrl[] = "calendar/v3/calendars/primary/events";
constexpr char kCalendarV3ColorUrl[] = "calendar/v3/colors";
constexpr char kTimeMaxParameterName[] = "timeMax";
constexpr char kTimeMinParameterName[] = "timeMin";
constexpr char kSingleEventsParameterName[] = "singleEvents";
constexpr char kMaxAttendeesParameterName[] = "maxAttendees";

}  // namespace

CalendarApiUrlGenerator::CalendarApiUrlGenerator() = default;

CalendarApiUrlGenerator::CalendarApiUrlGenerator(
    const CalendarApiUrlGenerator& src) = default;

CalendarApiUrlGenerator& CalendarApiUrlGenerator::operator=(
    const CalendarApiUrlGenerator& src) = default;

CalendarApiUrlGenerator::~CalendarApiUrlGenerator() = default;

GURL CalendarApiUrlGenerator::GetCalendarEventListUrl(
    const base::Time& start_time,
    const base::Time& end_time,
    bool single_events,
    absl::optional<int> max_attendees) const {
  GURL url = base_url_.Resolve(kCalendarV3EventsUrl);
  std::string start_time_string = util::FormatTimeAsString(start_time);
  std::string end_time_string = util::FormatTimeAsString(end_time);
  url = net::AppendOrReplaceQueryParameter(url, kTimeMinParameterName,
                                           start_time_string);
  url = net::AppendOrReplaceQueryParameter(url, kTimeMaxParameterName,
                                           end_time_string);
  url = net::AppendOrReplaceQueryParameter(url, kSingleEventsParameterName,
                                           single_events ? "true" : "false");
  if (max_attendees.has_value()) {
    url = net::AppendOrReplaceQueryParameter(
        url, kMaxAttendeesParameterName,
        base::NumberToString(max_attendees.value()));
  }
  return url;
}

GURL CalendarApiUrlGenerator::GetCalendarColorListUrl() const {
  GURL url = base_url_.Resolve(kCalendarV3ColorUrl);
  return url;
}

}  // namespace calendar
}  // namespace google_apis
