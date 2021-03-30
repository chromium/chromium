// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/reporting/browser_report_generator_ios.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/common/channel_info.h"

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

bool BrowserReportGeneratorIOS::IsExtendedStableChannel() {
  return false;  // Not supported on iOS.
}

void BrowserReportGeneratorIOS::GenerateBuildStateInfo(
    em::BrowserReport* report) {
  // Not used on iOS because there is no in-app auto-update.
}

void BrowserReportGeneratorIOS::GenerateProfileInfo(ReportType report_type,
                                                    em::BrowserReport* report) {
  for (const auto* entry : GetApplicationContext()
                               ->GetChromeBrowserStateManager()
                               ->GetLoadedBrowserStates()) {
    // Skip off-the-record profile.
    if (entry->IsOffTheRecord()) {
      continue;
    }

    em::ChromeUserProfileInfo* profile =
        report->add_chrome_user_profile_infos();
    profile->set_id(entry->GetStatePath().AsUTF8Unsafe());
    profile->set_name(entry->GetStatePath().BaseName().AsUTF8Unsafe());
    profile->set_is_detail_available(false);
  }
}

void BrowserReportGeneratorIOS::GeneratePluginsIfNeeded(
    ReportCallback callback,
    std::unique_ptr<em::BrowserReport> report) {
  // There are no plugins on iOS.
  std::move(callback).Run(std::move(report));
}

}  // namespace enterprise_reporting
