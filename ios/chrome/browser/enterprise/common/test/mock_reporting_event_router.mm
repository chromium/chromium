// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/common/test/mock_reporting_event_router.h"

MockReportingEventRouter::MockReportingEventRouter(
    enterprise_connectors::IOSRealtimeReportingClient* reporting_client)
    : ReportingEventRouter(reporting_client) {}

MockReportingEventRouter::~MockReportingEventRouter() = default;

// static
std::unique_ptr<KeyedService>
MockReportingEventRouter::BuildMockReportingEventRouter(ProfileIOS* profile) {
  return std::make_unique<MockReportingEventRouter>(
      enterprise_connectors::IOSRealtimeReportingClientFactory::GetForProfile(
          profile));
}
