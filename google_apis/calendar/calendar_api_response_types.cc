// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_response_types.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_value_converter.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"

namespace google_apis {

namespace calendar {

namespace {

// EventList
constexpr char kTimeZone[] = "timeZone";
constexpr char kCalendarEventListKind[] = "calendar#events";

// DateTime
constexpr char kDateTime[] = "dateTime";

// CalendarEvent
constexpr char kSummary[] = "summary";
constexpr char kStart[] = "start";
constexpr char kEnd[] = "end";
constexpr char kColorId[] = "colorId";
constexpr char kStatus[] = "status";
constexpr char kHtmlLink[] = "htmlLink";
constexpr char kCalendarEventKind[] = "calendar#event";
constexpr char kAttendees[] = "attendees";
constexpr char kAttendeesSelf[] = "self";
constexpr char kAttendeesResponseStatus[] = "responseStatus";

constexpr auto kEventStatuses =
    base::MakeFixedFlatMap<base::StringPiece, CalendarEvent::EventStatus>(
        {{"cancelled", CalendarEvent::EventStatus::kCancelled},
         {"confirmed", CalendarEvent::EventStatus::kConfirmed},
         {"tentative", CalendarEvent::EventStatus::kTentative}});

constexpr auto kAttendeesResponseStatuses =
    base::MakeFixedFlatMap<base::StringPiece, CalendarEvent::ResponseStatus>(
        {{"accepted", CalendarEvent::ResponseStatus::kAccepted},
         {"declined", CalendarEvent::ResponseStatus::kDeclined},
         {"needsAction", CalendarEvent::ResponseStatus::kNeedsAction},
         {"tentative", CalendarEvent::ResponseStatus::kTentative}});

// Converts the event status to `EventStatus`. Returns false when it fails
// (e.g. the value is structurally different from expected).
bool ConvertEventStatus(const base::Value* value,
                        CalendarEvent::EventStatus* result) {
  DCHECK(value);
  DCHECK(result);

  const auto* status = value->GetIfString();
  if (!status) {
    return false;
  }

  const auto* it = kEventStatuses.find(*status);
  if (it != kEventStatuses.end()) {
    *result = it->second;
  } else {
    *result = CalendarEvent::EventStatus::kUnknown;
  }
  return true;
}

// Finds the self response status from the attendees list. Returns false when
// it fails (e.g. the value is structurally different from expected).
bool GetSelfResponseStatusFromAttendees(const base::Value* value,
                                        CalendarEvent::ResponseStatus* result) {
  DCHECK(value);
  DCHECK(result);

  const auto* attendees = value->GetIfList();
  if (!attendees) {
    return false;
  }

  for (const auto& x : *attendees) {
    const auto* attendee = x.GetIfDict();
    if (!attendee) {
      return false;
    }

    const bool is_self = attendee->FindBool(kAttendeesSelf).value_or(false);
    if (!is_self)
      continue;

    const auto* responseStatus = attendee->FindString(kAttendeesResponseStatus);
    if (!responseStatus) {
      return false;
    }

    const auto* it = kAttendeesResponseStatuses.find(*responseStatus);
    if (it != kAttendeesResponseStatuses.end()) {
      *result = it->second;
    } else {
      *result = CalendarEvent::ResponseStatus::kUnknown;
    }
    break;
  }

  return true;
}

}  // namespace

DateTime::DateTime() = default;

DateTime::DateTime(const DateTime& src) = default;

DateTime& DateTime::operator=(const DateTime& src) = default;

DateTime::~DateTime() = default;

// static
void DateTime::RegisterJSONConverter(
    base::JSONValueConverter<DateTime>* converter) {
  converter->RegisterCustomField<base::Time>(kDateTime, &DateTime::date_time_,
                                             &util::GetTimeFromString);
}

// static
bool DateTime::CreateDateTimeFromValue(const base::Value* value,
                                       DateTime* time) {
  base::JSONValueConverter<DateTime> converter;
  if (!converter.Convert(*value, time)) {
    DVLOG(1) << "Unable to create: Invalid DateTime JSON!";
    return false;
  }
  return true;
}

CalendarEvent::CalendarEvent() = default;

CalendarEvent::~CalendarEvent() = default;

CalendarEvent::CalendarEvent(const CalendarEvent&) = default;

CalendarEvent& CalendarEvent::operator=(const CalendarEvent&) = default;

// static
void CalendarEvent::RegisterJSONConverter(
    base::JSONValueConverter<CalendarEvent>* converter) {
  converter->RegisterStringField(kApiResponseIdKey, &CalendarEvent::id_);
  converter->RegisterStringField(kSummary, &CalendarEvent::summary_);
  converter->RegisterStringField(kHtmlLink, &CalendarEvent::html_link_);
  converter->RegisterStringField(kColorId, &CalendarEvent::color_id_);
  converter->RegisterCustomValueField(kStatus, &CalendarEvent::status_,
                                      &ConvertEventStatus);
  converter->RegisterCustomValueField(kAttendees,
                                      &CalendarEvent::self_response_status_,
                                      &GetSelfResponseStatusFromAttendees);
  converter->RegisterCustomValueField(kStart, &CalendarEvent::start_time_,
                                      &DateTime::CreateDateTimeFromValue);
  converter->RegisterCustomValueField(kEnd, &CalendarEvent::end_time_,
                                      &DateTime::CreateDateTimeFromValue);
}

// static
std::unique_ptr<CalendarEvent> CalendarEvent::CreateFrom(
    const base::Value& value) {
  auto event = std::make_unique<CalendarEvent>();
  base::JSONValueConverter<CalendarEvent> converter;
  if (!IsResourceKindExpected(value, kCalendarEventKind) ||
      !converter.Convert(value, event.get())) {
    DVLOG(1) << "Unable to create: Invalid CalendarEvent JSON!";
    return nullptr;
  }

  return event;
}

int CalendarEvent::GetApproximateSizeInBytes() const {
  int total_bytes = 0;

  total_bytes += sizeof(CalendarEvent);
  total_bytes += id_.length();
  total_bytes += summary_.length();
  total_bytes += html_link_.length();
  total_bytes += color_id_.length();
  total_bytes += sizeof(status_);
  total_bytes += sizeof(self_response_status_);

  return total_bytes;
}

EventList::EventList() = default;

EventList::~EventList() = default;

// static
void EventList::RegisterJSONConverter(
    base::JSONValueConverter<EventList>* converter) {
  converter->RegisterStringField(kTimeZone, &EventList::time_zone_);
  converter->RegisterStringField(kApiResponseETagKey, &EventList::etag_);
  converter->RegisterStringField(kApiResponseKindKey, &EventList::kind_);
  converter->RegisterRepeatedMessage<CalendarEvent>(kApiResponseItemsKey,
                                                    &EventList::items_);
}

// static
std::unique_ptr<EventList> EventList::CreateFrom(const base::Value& value) {
  auto events = std::make_unique<EventList>();
  base::JSONValueConverter<EventList> converter;
  if (!IsResourceKindExpected(value, kCalendarEventListKind) ||
      !converter.Convert(value, events.get())) {
    DVLOG(1) << "Unable to create: Invalid EventList JSON!";
    return nullptr;
  }
  return events;
}

void EventList::InjectItemForTesting(std::unique_ptr<CalendarEvent> item) {
  items_.push_back(std::move(item));
}

}  // namespace calendar
}  // namespace google_apis
