// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_COMMON_TEST_MOCK_REPORTING_EVENT_ROUTER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_COMMON_TEST_MOCK_REPORTING_EVENT_ROUTER_H_

#import "components/enterprise/connectors/core/reporting_event_router.h"
#import "components/enterprise/data_controls/core/browser/verdict.h"
#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client.h"
#import "ios/chrome/browser/enterprise/connectors/reporting/ios_realtime_reporting_client_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "testing/gmock/include/gmock/gmock.h"

// Mock version for ReportingEventRouter.
class MockReportingEventRouter
    : public enterprise_connectors::ReportingEventRouter {
  using ReferrerChain =
      google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;

 public:
  explicit MockReportingEventRouter(
      enterprise_connectors::IOSRealtimeReportingClient* reporting_client);
  ~MockReportingEventRouter() override;

  MOCK_METHOD(void,
              OnUrlFilteringInterstitial,
              (const GURL& url,
               const std::string& threat_type,
               const safe_browsing::RTLookupResponse& response,
               const ReferrerChain& referrer_chain),
              (override));
  MOCK_METHOD(void,
              ReportPaste,
              (const data_controls::ClipboardContext&,
               const data_controls::Verdict&),
              (override));
  MOCK_METHOD(void,
              ReportPasteWarningBypassed,
              (const data_controls::ClipboardContext&,
               const data_controls::Verdict&),
              (override));
  MOCK_METHOD(void,
              ReportCopy,
              (const data_controls::ClipboardContext&,
               const data_controls::Verdict&),
              (override));
  MOCK_METHOD(void,
              ReportCopyWarningBypassed,
              (const data_controls::ClipboardContext&,
               const data_controls::Verdict&),
              (override));

  // Static factory method to build a MockReportingEventRouter.
  static std::unique_ptr<KeyedService> BuildMockReportingEventRouter(
      ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_COMMON_TEST_MOCK_REPORTING_EVENT_ROUTER_H_
