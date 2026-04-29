// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_

#import "components/enterprise/connectors/core/cloud_content_scanning/files_request_handler_base.h"
#import "components/enterprise/connectors/core/common.h"

class GURL;
class ReportingEventRouter;
class ContentAnalysisInfoBase;

namespace safe_browsing {
class RTLookupResponse;
}  // namespace safe_browsing

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

// Map RequestHandler Scanning Result to Event Result.
EventResult GetEventResult(const RequestHandlerResult& result);

// Reports a URL filtering event (e.g., interstitial shown, bypassed) to the
// enterprise reporting system.
//
// Parameters:
//   event_type: The type of event that occurred (see UrlFilteringEventType).
//   page_url: The URL that triggered the event.
//   rt_lookup_response: Contains information about the event.
//   event_router: The router to send the report.
void ReportEnterpriseUrlFilteringEvent(
    UrlFilteringEventType event_type,
    const GURL& page_url,
    const safe_browsing::RTLookupResponse& rt_lookup_response,
    ReportingEventRouter* event_router);

// Reports a dangerous download event with the content analysis info and
// response, the event result can be BLOCK or WARN.
void MaybeReportDangerousDownloadEvent(
    const ContentAnalysisInfoBase& info,
    const ContentAnalysisResponse& response,
    const base::FilePath& path,
    const FilesRequestHandlerBase::FileInfo& file_info,
    EventResult result,
    ReportingEventRouter* router);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_UTIL_H_
