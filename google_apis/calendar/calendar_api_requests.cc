// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_requests.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace calendar {

namespace {

// For events without an event color, fill the colorId field with the color ID
// provided by the calendar.
void FillEmptyColorFields(EventList* events, const std::string& color) {
  for (auto& event : events->items()) {
    if (event->color_id().empty()) {
      // Events and calendars have some shared color IDs associated with
      // different colors. Here we prepend the injected ID with a marker
      // to indicate that the color ID does not represent an original event
      // color.
      event->set_color_id(kInjectedColorIdPrefix + color);
    }
  }
}

constexpr char kFieldsParameterName[] = "fields";

// According to the docs
// (https://developers.google.com/calendar/api/v3/reference/events/list), it
// should return the participant/requester only as an attendee.
constexpr int kMaxAttendees = 1;

// Requested maximum number of calendars returned. The default value is 100.
// Although the events shown to the user will be limited to a fixed number
// of selected calendars, it is best to be thorough here and filter later.
constexpr int kMaxCalendars = 250;

// Requested number of events returned on one result page.
// The default value on the Calendar API side is 250 events per page and some
// users have >250 events per month.
// As a short term solution increasing the number of events per page to the
// maximum allowed number (2500 / 30 = 80+ events per day should be more than
// enough).
// TODO(crbug.com/40862361): Implement pagination using `nextPageToken` from the
// response.
constexpr int kMaxResults = 2500;

// Requested fields to be returned in the CalendarList result.
constexpr char kCalendarListFields[] =
    "etag,kind,items(kind,id,summary,colorId,selected,primary)";

// Requested fields to be returned in the Event list result.
std::string GetCalendarEventListFields(bool include_attachments) {
  return base::StrCat(
      {"timeZone,etag,kind,items(id,kind,summary,colorId,"
       "status,start(date),end(date),start(dateTime),end(dateTime),htmlLink,"
       "attendees(responseStatus,self),attendeesOmitted,"
       "conferenceData(conferenceId,entryPoints(entryPointType,uri)),"
       "creator(self),location",
       include_attachments ? ",attachments(title,fileUrl,iconLink,fileId)" : "",
       ")"});
}

}  // namespace

CalendarApiGetRequest::CalendarApiGetRequest(RequestSender* sender,
                                             const std::string& fields)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      fields_(fields) {}

CalendarApiGetRequest::~CalendarApiGetRequest() = default;

GURL CalendarApiGetRequest::GetURL() const {
  GURL url = GetURLInternal();
  if (!fields_.empty()) {
    url =
        net::AppendOrReplaceQueryParameter(url, kFieldsParameterName, fields_);
  }
  return url;
}

// Maps calendar api error reason to code. See
// https://developers.google.com/calendar/api/guides/errors.
ApiErrorCode CalendarApiGetRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  const char kErrorReasonRateLimitExceeded[] = "rateLimitExceeded";

  // The rateLimitExceeded errors can return either 403 or 429 error codes, but
  // we want to treat them in the same way.
  if (reason == kErrorReasonRateLimitExceeded)
    return google_apis::HTTP_FORBIDDEN;
  return code;
}

bool CalendarApiGetRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return IsSuccessfulCalendarApiErrorCode(error);
}

CalendarApiCalendarListRequest::CalendarApiCalendarListRequest(
    RequestSender* sender,
    const CalendarApiUrlGenerator& url_generator,
    CalendarListCallback callback)
    : CalendarApiGetRequest(sender, kCalendarListFields),
      callback_(std::move(callback)),
      url_generator_(url_generator) {
  CHECK(!callback_.is_null());
}

CalendarApiCalendarListRequest::~CalendarApiCalendarListRequest() = default;

GURL CalendarApiCalendarListRequest::GetURLInternal() const {
  return url_generator_.GetCalendarListUrl(/*max_results=*/kMaxCalendars);
}

void CalendarApiCalendarListRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CalendarApiCalendarListRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&CalendarApiCalendarListRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr(), error));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void CalendarApiCalendarListRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  std::move(callback_).Run(error, std::unique_ptr<CalendarList>());
}

// static
std::unique_ptr<CalendarList> CalendarApiCalendarListRequest::Parse(
    std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);

  return value ? CalendarList::CreateFrom(*value) : nullptr;
}

void CalendarApiCalendarListRequest::OnDataParsed(
    ApiErrorCode error,
    std::unique_ptr<CalendarList> calendars) {
  if (!calendars) {
    error = PARSE_ERROR;
  }
  std::move(callback_).Run(error, std::move(calendars));
  OnProcessURLFetchResultsComplete();
}

CalendarApiEventsRequest::CalendarApiEventsRequest(
    RequestSender* sender,
    const CalendarApiUrlGenerator& url_generator,
    CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::string& calendar_id,
    const std::string& calendar_color_id)
    : CalendarApiGetRequest(
          sender,
          GetCalendarEventListFields(/*include_attachments=*/false)),
      callback_(std::move(callback)),
      url_generator_(url_generator),
      calendar_color_id_(calendar_color_id),
      calendar_id_(calendar_id),
      end_time_(end_time),
      max_attendees_(kMaxAttendees),
      start_time_(start_time) {
  CHECK(!callback_.is_null());
}

CalendarApiEventsRequest::CalendarApiEventsRequest(
    RequestSender* sender,
    const CalendarApiUrlGenerator& url_generator,
    CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time,
    bool include_attachments)
    : CalendarApiGetRequest(sender,
                            GetCalendarEventListFields(include_attachments)),
      callback_(std::move(callback)),
      url_generator_(url_generator),
      calendar_id_(kPrimaryCalendarId),
      end_time_(end_time),
      max_attendees_(kMaxAttendees),
      start_time_(start_time) {
  CHECK(!callback_.is_null());
}

CalendarApiEventsRequest::CalendarApiEventsRequest(
    RequestSender* sender,
    const CalendarApiUrlGenerator& url_generator,
    CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time,
    const std::vector<EventType>& event_types,
    const std::string& experiment,
    const std::string& order_by,
    bool include_attachments)
    : CalendarApiGetRequest(sender,
                            GetCalendarEventListFields(include_attachments)),
      callback_(std::move(callback)),
      url_generator_(url_generator),
      calendar_id_(kPrimaryCalendarId),
      end_time_(end_time),
      event_types_(event_types),
      experiment_(experiment),
      order_by_(order_by),
      start_time_(start_time) {
  CHECK(!callback_.is_null());
}

CalendarApiEventsRequest::~CalendarApiEventsRequest() = default;

GURL CalendarApiEventsRequest::GetURLInternal() const {
  return url_generator_.GetCalendarEventListUrl(
      calendar_id_, start_time_, end_time_,
      /*single_events=*/true,
      /*max_attendees=*/max_attendees_,
      /*max_results=*/kMaxResults, event_types_, experiment_, order_by_);
}

void CalendarApiEventsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CalendarApiEventsRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&CalendarApiEventsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr(), error));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void CalendarApiEventsRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  std::move(callback_).Run(error, std::unique_ptr<EventList>());
}

// static
std::unique_ptr<EventList> CalendarApiEventsRequest::Parse(std::string json) {
  std::unique_ptr<base::Value> value = ParseJson(json);

  return value ? EventList::CreateFrom(*value) : nullptr;
}

void CalendarApiEventsRequest::OnDataParsed(ApiErrorCode error,
                                            std::unique_ptr<EventList> events) {
  if (!events) {
    error = PARSE_ERROR;
  }
  if (!calendar_color_id_.empty()) {
    FillEmptyColorFields(events.get(), calendar_color_id_);
  }
  std::move(callback_).Run(error, std::move(events));
  OnProcessURLFetchResultsComplete();
}

}  // namespace calendar
}  // namespace google_apis
