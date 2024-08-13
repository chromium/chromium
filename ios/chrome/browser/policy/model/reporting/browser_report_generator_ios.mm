// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/browser_report_generator_ios.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/common/channel_info.h"

namespace em = ::enterprise_management;

namespace enterprise_reporting {

BrowserReportGeneratorIOS::BrowserReportGeneratorIOS() = default;

BrowserReportGeneratorIOS::~BrowserReportGeneratorIOS() = default;

std::string BrowserReportGeneratorIOS::GetExecutablePath() {
  NSBundle* baseBundle = base::apple::OuterBundle();
  return base::SysNSStringToUTF8([baseBundle bundleIdentifier]);
}

version_info::Channel BrowserReportGeneratorIOS::GetChannel() {
  return ::GetChannel();
}

std::vector<BrowserReportGenerator::ReportedProfileData>
BrowserReportGeneratorIOS::GetReportedProfiles() {
  std::vector<BrowserReportGenerator::ReportedProfileData> reportedProfileData;
  for (const auto* browser_state : GetApplicationContext()
                                       ->GetChromeBrowserStateManager()
                                       ->GetLoadedBrowserStates()) {
    // ChromeBrowserStateManager should not return off-the-record BrowserStates.
    CHECK(!browser_state->IsOffTheRecord());
    reportedProfileData.push_back({
        browser_state->GetStatePath().AsUTF8Unsafe(),
        browser_state->GetBrowserStateName(),
    });
  }

  return reportedProfileData;
}

bool BrowserReportGeneratorIOS::IsExtendedStableChannel() {
  return false;  // Not supported on iOS.
}

void BrowserReportGeneratorIOS::GenerateBuildStateInfo(
    em::BrowserReport* report) {
  // Not used on iOS because there is no in-app auto-update.
}

}  // namespace enterprise_reporting
