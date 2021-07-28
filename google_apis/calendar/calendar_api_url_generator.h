// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CALENDAR_CALENDAR_API_URL_GENERATOR_H_
#define GOOGLE_APIS_CALENDAR_CALENDAR_API_URL_GENERATOR_H_

#include <string>

#include "base/time/time.h"
#include "google_apis/gaia/gaia_urls.h"
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
  GURL GetCalendarEventListUrl(const base::Time& start_time,
                               const base::Time& end_time) const;

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
