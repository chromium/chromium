// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CALENDAR_CALENDAR_API_URL_GENERATOR_H_
#define GOOGLE_APIS_CALENDAR_CALENDAR_API_URL_GENERATOR_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace google_apis {

namespace calendar {

// This class is used to generate URLs for communicating with calendar api
// servers for production, and a local server for testing.
class CalendarApiUrlGenerator {
 public:
  CalendarApiUrlGenerator();
  CalendarApiUrlGenerator(const CalendarApiUrlGenerator& src);
  CalendarApiUrlGenerator& operator=(const CalendarApiUrlGenerator& src);
  ~CalendarApiUrlGenerator();

  // Returns a URL to fetch a list of calendar events.
  // |start_time|    Start time of the event window
  // |end_time|      End time of the aforementioned window
  // |single_events| If true, expand recurring events into instances and only
  //                 return single one-off events and instances of recurring
  //                 events, but not the underlying recurring events
  //                 themselves
  // |max_attendees| The maximum number of attendees to include in the response.
  //                 If there are more than the specified number of attendees,
  //                 only the participant is returned. Optional.
  // |max_results|   Maximum number of events returned on one result page.
  //                 Optional.
  GURL GetCalendarEventListUrl(const base::Time& start_time,
                               const base::Time& end_time,
                               bool single_events,
                               absl::optional<int> max_attendees,
                               absl::optional<int> max_results) const;

  // Returns a URL to fetch a map of calendar color id to color code.
  GURL GetCalendarColorListUrl() const;

  // The base url can be set here. It defaults to the production base url.
  void SetBaseUrlForTesting(const std::string& url) { base_url_ = GURL(url); }

 private:
  GURL base_url_{GaiaUrls::GetInstance()->google_apis_origin_url()};
};

}  // namespace calendar
}  // namespace google_apis

#endif  // GOOGLE_APIS_CALENDAR_CALENDAR_API_URL_GENERATOR_H_
