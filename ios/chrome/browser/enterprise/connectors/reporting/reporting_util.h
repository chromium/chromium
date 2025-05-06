// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_

class GURL;

namespace web {
class WebState;
}  // namespace web

namespace enterprise_connectors {

// Enum representing the type of URL filtering event being reported.
enum class UrlFilteringEventType {
  // Interstitial for a blocked URL was shown.
  kBlockedSeen,
  // Interstitial for a flagged URL was shown.
  kWarnedSeen,
  // User chose to bypass the warning interstitial.
  kBypassed,
};

// Reports a URL filtering event (e.g., interstitial shown, bypassed) to the
// enterprise reporting system.
//
// Parameters:
//   event_type: The type of event that occurred (see UrlFilteringEventType).
//   page_url: The URL that triggered the event.
//   web_state: The WebState associated with the event.
void ReportEnterpriseUrlFilteringEvent(UrlFilteringEventType event_type,
                                       const GURL& page_url,
                                       web::WebState* web_state);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_
