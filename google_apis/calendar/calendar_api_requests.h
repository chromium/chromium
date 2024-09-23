// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_
#define GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/calendar/calendar_api_url_generator.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {

namespace calendar {

// A marker to indicate an event has been injected with its calendar's colorId.
inline const std::string kInjectedColorIdPrefix = "c";
inline constexpr char kPrimaryCalendarId[] = "primary";

// Callback used for requests that the server returns Calendar List
// data formatted into JSON value.
using CalendarListCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<CalendarList> calendars)>;

// Callback used for requests that the server returns Events data
// formatted into JSON value.
using CalendarEventListCallback =
    base::OnceCallback<void(ApiErrorCode error,
                            std::unique_ptr<EventList> events)>;

// This is base class of the Calendar API related requests.
class CalendarApiGetRequest : public UrlFetchRequestBase {
 public:
  // `fields` is an optional request parameter that allows you to specify the
  // fields you want returned in the response data. Documentation:
  // https://developers.google.com/calendar/api/guides/performance?hl=en#partial-response
  CalendarApiGetRequest(RequestSender* sender, const std::string& fields);

  CalendarApiGetRequest(const CalendarApiGetRequest&) = delete;
  CalendarApiGetRequest& operator=(const CalendarApiGetRequest&) = delete;
  ~CalendarApiGetRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;

  // Derived classes should override GetURLInternal instead of GetURL()
  // directly since fields are appended in the GetURL() method.
  virtual GURL GetURLInternal() const = 0;

 private:
  // Optional parameter in the request.
  std::string fields_;
};

// Request to fetch the list of the user's calendars.
class CalendarApiCalendarListRequest : public CalendarApiGetRequest {
 public:
  CalendarApiCalendarListRequest(RequestSender* sender,
                                 const CalendarApiUrlGenerator& url_generator,
                                 CalendarListCallback callback);
  CalendarApiCalendarListRequest(const CalendarApiCalendarListRequest&) =
      delete;
  CalendarApiCalendarListRequest& operator=(
      const CalendarApiCalendarListRequest&) = delete;
  ~CalendarApiCalendarListRequest() override;

 protected:
  // CalendarApiGetRequest:
  GURL GetURLInternal() const override;

  // UrlFetchRequestBase:
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;

  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  // Parses the Calendar List result to a CalendarList.
  static std::unique_ptr<CalendarList> Parse(std::string json);

  // Receives the parsed calendar list and invokes the callback.
  void OnDataParsed(ApiErrorCode error,
                    std::unique_ptr<CalendarList> calendars);

  CalendarListCallback callback_;
  const CalendarApiUrlGenerator url_generator_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CalendarApiCalendarListRequest> weak_ptr_factory_{this};
};

// Request to fetch calendar events. By default, an event fetch for the primary
// calendar is requested. If a calendar ID is passed, an event fetch is
// requested for the calendar matching that ID.
// Only fetches a maximum of 1 attendee per event.
// |url_generator|  The UrlGenerator to use for the request.
// |callback|       The callback to send the parsed |EventList| to when the
//                  request is complete.
// |start_time|     The minimum time to fetch events for. This will filter
//                  out events with an end time before this value.
// |end_time|       The maximum time to fetch events for. This will filter
//                  out events with a start time after this value.
class CalendarApiEventsRequest : public CalendarApiGetRequest {
 public:
  CalendarApiEventsRequest(RequestSender* sender,
                           const CalendarApiUrlGenerator& url_generator,
                           CalendarEventListCallback callback,
                           const base::Time& start_time,
                           const base::Time& end_time,
                           const std::string& calendar_id,
                           const std::string& calendar_color_id);
  CalendarApiEventsRequest(RequestSender* sender,
                           const CalendarApiUrlGenerator& url_generator,
                           CalendarEventListCallback callback,
                           const base::Time& start_time,
                           const base::Time& end_time,
                           bool include_attachments = false);
  // Creates a CalendarApiEventsRequest for the user's primary calendar with
  // extra params and does not limit the number of attendees per event.
  // |event_types|          A vector of |EventType| to filter
  //                        for. If the vector is empty, no filtering by
  //                        event_types will occur.
  // |experiment|           A string indicating an experiment param to add to
  //                        the query. This does not filter further, but allows
  //                        for separating requests by project in the backend.
  // |order_by|             The field to order the results by. This can be
  //                        "startTime" or "updated".
  // |include_attachments|  Boolean of whether to include attachments in the
  //                        response object. Default is true.
  CalendarApiEventsRequest(RequestSender* sender,
                           const CalendarApiUrlGenerator& url_generator,
                           CalendarEventListCallback callback,
                           const base::Time& start_time,
                           const base::Time& end_time,
                           const std::vector<EventType>& event_types,
                           const std::string& experiment,
                           const std::string& order_by,
                           bool include_attachments = true);
  CalendarApiEventsRequest(const CalendarApiEventsRequest&) = delete;
  CalendarApiEventsRequest& operator=(const CalendarApiEventsRequest&) = delete;
  ~CalendarApiEventsRequest() override;

 protected:
  // CalendarApiGetRequest:
  GURL GetURLInternal() const override;

  // UrlFetchRequestBase:
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;

  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  // Parses the |json| string to EventList.
  static std::unique_ptr<EventList> Parse(std::string json);

  // Receives the parsed result and invokes the callback.
  void OnDataParsed(ApiErrorCode error, std::unique_ptr<EventList> events);

  CalendarEventListCallback callback_;
  const CalendarApiUrlGenerator url_generator_;

  const std::string calendar_color_id_;
  const std::string calendar_id_;
  const base::Time end_time_;
  const std::vector<EventType> event_types_;
  const std::string experiment_;
  const std::optional<int> max_attendees_;
  const std::string order_by_;
  const base::Time start_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CalendarApiEventsRequest> weak_ptr_factory_{this};
};

}  // namespace calendar
}  // namespace google_apis

#endif  // GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_
