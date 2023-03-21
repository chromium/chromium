// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/reporting/browser_report_generator_ios.h"

#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace em = ::enterprise_management;

namespace enterprise_reporting {

BrowserReportGeneratorIOS::BrowserReportGeneratorIOS() = default;

BrowserReportGeneratorIOS::~BrowserReportGeneratorIOS() = default;

std::string BrowserReportGeneratorIOS::GetExecutablePath() {
  NSBundle* baseBundle = base::mac::OuterBundle();
  return base::SysNSStringToUTF8([baseBundle bundleIdentifier]);
}

version_info::Channel BrowserReportGeneratorIOS::GetChannel() {
  return ::GetChannel();
}

std::vector<BrowserReportGenerator::ReportedProfileData>
BrowserReportGeneratorIOS::GetReportedProfiles() {
  std::vector<BrowserReportGenerator::ReportedProfileData> reportedProfileData;
  for (const auto* entry : GetApplicationContext()
                               ->GetChromeBrowserStateManager()
                               ->GetLoadedBrowserStates()) {
    // Skip off-the-record profile.
    if (entry->IsOffTheRecord()) {
      continue;
    }

    reportedProfileData.push_back(
        {entry->GetStatePath().AsUTF8Unsafe(),
         entry->GetStatePath().BaseName().AsUTF8Unsafe()});
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
