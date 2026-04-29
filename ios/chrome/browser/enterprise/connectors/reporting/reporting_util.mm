// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/reporting/reporting_util.h"

#import <string.h>

#import "components/enterprise/common/proto/connectors.pb.h"
#import "components/enterprise/connectors/core/content_analysis_info_base.h"
#import "components/enterprise/connectors/core/features.h"
#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "url/gurl.h"

namespace {

using enterprise_connectors::UrlFilteringEventType;

// Converts the UrlFilteringEventType enum to the string expected by the
// ReportingEventRouter.
std::string GetEventTypeString(UrlFilteringEventType event_type) {
  switch (event_type) {
    case UrlFilteringEventType::kBlockedSeen:
      return "ENTERPRISE_BLOCKED_SEEN";
    case UrlFilteringEventType::kWarnedSeen:
      return "ENTERPRISE_WARNED_SEEN";
    case UrlFilteringEventType::kBypassed:
      return "ENTERPRISE_WARNED_BYPASS";
  }
}

}  // namespace

namespace enterprise_connectors {

EventResult GetEventResult(const RequestHandlerResult& result) {
  EventResult event_result = EventResult::UNKNOWN;
  switch (result.final_result) {
    case FinalContentAnalysisResult::SUCCESS:
      event_result = EventResult::ALLOWED;
      break;
    case FinalContentAnalysisResult::WARNING:
      event_result = EventResult::WARNED;
      break;
    case FinalContentAnalysisResult::LARGE_FILES:
    case FinalContentAnalysisResult::FAILURE:
    case FinalContentAnalysisResult::FAIL_CLOSED:
    case FinalContentAnalysisResult::CANCELLED:
      event_result = EventResult::BLOCKED;
      break;
    case FinalContentAnalysisResult::FORCE_SAVE_TO_CLOUD:
      event_result = EventResult::FORCED_SAVE_TO_CLOUD;
      break;
    case FinalContentAnalysisResult::ENCRYPTED_FILES:
      // Encrypted Files are not supported on iOS.
      NOTREACHED();
  }
  return event_result;
}

void ReportEnterpriseUrlFilteringEvent(
    UrlFilteringEventType event_type,
    const GURL& page_url,
    const safe_browsing::RTLookupResponse& rt_lookup_response,
    ReportingEventRouter* event_router) {
  CHECK(event_router);

  // ReferrerChain is not supported on ios for now.
  event_router->OnUrlFilteringInterstitial(
      page_url, GetEventTypeString(event_type), rt_lookup_response,
      /*referrer_chain=*/{});
}

void MaybeReportDangerousDownloadEvent(
    const ContentAnalysisInfoBase& info,
    const ContentAnalysisResponse& response,
    const base::FilePath& path,
    const FilesRequestHandlerBase::FileInfo& file_info,
    EventResult result,
    ReportingEventRouter* router) {
  CHECK(router);

  // If the ContentAnalysisResponse has a deep scanning malware verdict, then it
  // means the dangerous file has already been reported.
  if (ContainsMalwareVerdict(response)) {
    return;
  }

  router->OnDangerousDownloadEvent(
      info.url(), info.tab_url(),
      /*source=*/"", /*destination=*/"", path.AsUTF8Unsafe(),
      file_info.sha256_or_cb,
      /*threat_type=*/kDangerousFileTypeDownloadThreatType, file_info.mime_type,
      kFileDownloadDataTransferEventTrigger, response.request_token(),
      /*content_transfer_method=*/"", file_info.size, info.referrer_chain(),
      info.frame_url_chain(), result);
}

}  // namespace enterprise_connectors
