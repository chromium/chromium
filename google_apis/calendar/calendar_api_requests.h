// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_
#define GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_

#include "base/time/time.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "google_apis/calendar/calendar_api_url_generator.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {

namespace calendar {

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

// Request to fetch calendar events.
class CalendarApiEventsRequest : public CalendarApiGetRequest {
 public:
  CalendarApiEventsRequest(RequestSender* sender,
                           const CalendarApiUrlGenerator& url_generator,
                           CalendarEventListCallback callback,
                           const base::Time& start_time,
                           const base::Time& end_time);
  CalendarApiEventsRequest(const CalendarApiEventsRequest&) = delete;
  CalendarApiEventsRequest& operator=(const CalendarApiEventsRequest&) = delete;
  ~CalendarApiEventsRequest() override;

 protected:
  // CalendarApiGetRequest:
  GURL GetURLInternal() const override;

  // UrlFetchRequestBase:
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;

  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  // Parses the |json| string to EventList.
  static std::unique_ptr<EventList> Parse(std::string json);

  // Receives the parsed result and invokes the callback.
  void OnDataParsed(ApiErrorCode error, std::unique_ptr<EventList> events);

  CalendarEventListCallback callback_;
  const CalendarApiUrlGenerator url_generator_;

  const base::Time start_time_;
  const base::Time end_time_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CalendarApiEventsRequest> weak_ptr_factory_{this};
};

}  // namespace calendar
}  // namespace google_apis

#endif  // GOOGLE_APIS_CALENDAR_CALENDAR_API_REQUESTS_H_
