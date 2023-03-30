// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_response_types.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_value_converter.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/common/parser_util.h"
#include "google_apis/common/time_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace google_apis {

namespace calendar {

namespace {

// EventList
constexpr char kCalendarEventListKind[] = "calendar#events";
constexpr char kTimeZone[] = "timeZone";

// DateTime
constexpr char kDateTime[] = "dateTime";

// Date
constexpr char kDate[] = "date";

// CalendarEvent
constexpr char kAttendees[] = "attendees";
constexpr char kAttendeesOmitted[] = "attendeesOmitted";
constexpr char kAttendeesResponseStatus[] = "responseStatus";
constexpr char kAttendeesSelf[] = "self";
constexpr char kCalendarEventKind[] = "calendar#event";
constexpr char kColorId[] = "colorId";
constexpr char kEnd[] = "end";
constexpr char kHtmlLink[] = "htmlLink";
constexpr char kPathToCreatorSelf[] = "creator.self";
constexpr char kStart[] = "start";
constexpr char kStatus[] = "status";
constexpr char kSummary[] = "summary";

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

// ConferenceData
constexpr char kConferenceDataEntryPoints[] = "conferenceData.entryPoints";
constexpr char kEntryPointType[] = "entryPointType";
constexpr char kVideoConferenceValue[] = "video";
constexpr char kEntryPointUri[] = "uri";

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

// Returns user's self response status on the event, or `absl::nullopt` in case
// the passed value is structurally different from expected.
absl::optional<CalendarEvent::ResponseStatus> CalculateSelfResponseStatus(
    const base::Value& value) {
  const auto* event = value.GetIfDict();
  if (!event)
    return absl::nullopt;

  const auto* attendees_raw_value = event->Find(kAttendees);
  if (!attendees_raw_value) {
    // It's okay not to have `attendees` field in the response, it's possible
    // in two cases:
    // - this is a personal event of the `default` type without other
    // attendees;
    // - user invited 2+ guests and removed themselves from the event (also,
    // the `attendeesOmitted` flag will be set to true).

    const bool is_self_created =
        event->FindBoolByDottedPath(kPathToCreatorSelf).value_or(false);
    const bool has_omitted_attendees =
        event->FindBool(kAttendeesOmitted).value_or(false);

    if (is_self_created && !has_omitted_attendees) {
      // It's not possible to create a personal event and mark it as
      // tentative or declined.
      return CalendarEvent::ResponseStatus::kAccepted;
    }

    return CalendarEvent::ResponseStatus::kUnknown;
  }

  const auto* attendees = attendees_raw_value->GetIfList();
  if (!attendees)
    return absl::nullopt;

  for (const auto& x : *attendees) {
    const auto* attendee = x.GetIfDict();
    if (!attendee)
      return absl::nullopt;

    const bool is_self = attendee->FindBool(kAttendeesSelf).value_or(false);
    if (!is_self) {
      // A special case when user invited 1 more guest and removed themselves
      // from the event. In this case that user will be returned as an
      // attendee (the total number of attendees <= the requested number), safe
      // to ignore this item.
      continue;
    }

    const auto* responseStatus = attendee->FindString(kAttendeesResponseStatus);
    if (!responseStatus)
      return absl::nullopt;

    const auto* it = kAttendeesResponseStatuses.find(*responseStatus);
    if (it != kAttendeesResponseStatuses.end()) {
      return it->second;
    }
  }

  return CalendarEvent::ResponseStatus::kUnknown;
}

// Pulls the video conference URI out of the conferenceData field, if there is
// one on the event. Returns the first one it finds or an empty GURL if there is
// none.
GURL GetConferenceDataUri(const base::Value::Dict& dict) {
  const auto* entry_points =
      dict.FindListByDottedPath(kConferenceDataEntryPoints);
  if (!entry_points) {
    return GURL();
  }

  const auto video_conference_entry_point = base::ranges::find_if(
      entry_points->begin(), entry_points->end(), [](const auto& entry_point) {
        const std::string* entry_point_type =
            entry_point.GetDict().FindString(kEntryPointType);
        if (!entry_point_type) {
          return false;
        }
        return *entry_point_type == kVideoConferenceValue;
      });

  if (video_conference_entry_point == entry_points->end()) {
    return GURL();
  }

  const std::string* entry_point_uri =
      video_conference_entry_point->GetDict().FindString(kEntryPointUri);
  if (!entry_point_uri) {
    return GURL();
  }
  const GURL entry_point_url = GURL(*entry_point_uri);
  if (entry_point_url.is_valid()) {
    return entry_point_url;
  }

  return GURL();
}

// Converts the `items` field from the response. This method helps to use the
// custom conversion entrypoint `CalendarEvent::CreateFrom`.
// Returns false when the conversion fails (e.g. the value is structurally
// different from expected).
// Returns true otherwise.
bool ConvertResponseItems(const base::Value* value, CalendarEvent* event) {
  base::JSONValueConverter<CalendarEvent> converter;

  if (!IsResourceKindExpected(*value, kCalendarEventKind) ||
      !converter.Convert(*value, event)) {
    DVLOG(1) << "Unable to create: Invalid CalendarEvent JSON!";
    return false;
  }

  auto self_response_status = CalculateSelfResponseStatus(*value);
  if (self_response_status.has_value()) {
    event->set_self_response_status(self_response_status.value());
  }

  GURL conference_data_uri = GetConferenceDataUri(value->GetDict());
  event->set_conference_data_uri(conference_data_uri);

  return true;
}

bool IsAllDayEvent(const base::Value* value, bool* result) {
  *result = value->GetDict().Find("date") != nullptr;
  return result;
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
  converter->RegisterCustomField<base::Time>(kDate, &DateTime::date_time_,
                                             &util::GetDateOnlyFromString);
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
  converter->RegisterCustomValueField(kStart, &CalendarEvent::start_time_,
                                      &DateTime::CreateDateTimeFromValue);
  converter->RegisterCustomValueField(kEnd, &CalendarEvent::end_time_,
                                      &DateTime::CreateDateTimeFromValue);
  converter->RegisterCustomValueField(kStart, &CalendarEvent::all_day_event_,
                                      &IsAllDayEvent);
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
  converter->RegisterRepeatedCustomValue<CalendarEvent>(
      kApiResponseItemsKey, &EventList::items_, &ConvertResponseItems);
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
