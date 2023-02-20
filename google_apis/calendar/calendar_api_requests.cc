// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_requests.h"

#include <stddef.h>

#include "base/values.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace calendar {

constexpr char kFieldsParameterName[] = "fields";

// According to the docs
// (https://developers.google.com/calendar/api/v3/reference/events/list), it
// should return the participant/requester only as an attendee.
constexpr int kMaxAttendees = 1;

// Requested number of events returned on one result page.
// The default value on the Calendar API side is 250 events per page and some
// users have >250 events per month.
// As a short term solution increasing the number of events per page to the
// maximum allowed number (2500 / 30 = 80+ events per day should be more than
// enough).
// TODO(crbug.com/1359388): Implement pagination using `nextPageToken` from the
// response.
constexpr int kMaxResults = 2500;

constexpr char kCalendarEventListFields[] =
    "timeZone,etag,kind,items(id,kind,summary,colorId,"
    "status,start(date),end(date),start(dateTime),end(dateTime),htmlLink,"
    "attendees(responseStatus,self),attendeesOmitted,hangoutLink,"
    "creator(self))";

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

CalendarApiEventsRequest::CalendarApiEventsRequest(
    RequestSender* sender,
    const CalendarApiUrlGenerator& url_generator,
    CalendarEventListCallback callback,
    const base::Time& start_time,
    const base::Time& end_time)
    : CalendarApiGetRequest(sender, kCalendarEventListFields),
      callback_(std::move(callback)),
      url_generator_(url_generator),
      start_time_(start_time),
      end_time_(end_time) {
  DCHECK(!callback_.is_null());
}

CalendarApiEventsRequest::~CalendarApiEventsRequest() = default;

GURL CalendarApiEventsRequest::GetURLInternal() const {
  return url_generator_.GetCalendarEventListUrl(start_time_, end_time_,
                                                /*single_events=*/true,
                                                /*max_attendees=*/kMaxAttendees,
                                                /*max_results=*/kMaxResults);
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

void CalendarApiEventsRequest::OnDataParsed(ApiErrorCode error,
                                            std::unique_ptr<EventList> events) {
  if (!events)
    error = PARSE_ERROR;
  std::move(callback_).Run(error, std::move(events));
  OnProcessURLFetchResultsComplete();
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

}  // namespace calendar
}  // namespace google_apis
