// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/browser_report_generator_ios.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/proto/device_management_backend.pb.h"
#import "ios/chrome/browser/policy/model/reporting/features.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/common/channel_info.h"

namespace em = ::enterprise_management;

namespace enterprise_reporting {

namespace {

std::vector<std::string> GetKnownProfilesNames(ProfileManagerIOS* manager) {
  std::vector<std::string> names;
  manager->GetProfileAttributesStorage()->IterateOverProfileAttributes(
      base::BindRepeating(
          [](std::vector<std::string>& names,
             const ProfileAttributesIOS& attrs) {
            names.push_back(attrs.GetProfileName());
          },
          std::ref(names)));
  return names;
}

std::vector<BrowserReportGenerator::ReportedProfileData>
GetReportDataForKnownProfiles(ProfileManagerIOS* manager) {
  std::vector<BrowserReportGenerator::ReportedProfileData> result;
  std::ranges::transform(GetKnownProfilesNames(manager),
                         std::back_inserter(result),
                         [](const std::string& name) {
                           return BrowserReportGenerator::ReportedProfileData(
                               SanitizeProfilePath(name), name);
                         });
  return result;
}

std::vector<BrowserReportGenerator::ReportedProfileData>
GetReportDataForLoadedProfiles(ProfileManagerIOS* manager) {
  std::vector<BrowserReportGenerator::ReportedProfileData> result;
  std::ranges::transform(manager->GetLoadedProfiles(),
                         std::back_inserter(result), [](ProfileIOS* profile) {
                           CHECK(!profile->IsOffTheRecord());
                           return BrowserReportGenerator::ReportedProfileData(
                               SanitizeProfilePath(profile->GetProfileName()),
                               profile->GetProfileName());
                         });
  return result;
}

}  // namespace

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
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();

  if (base::FeatureList::IsEnabled(
          enterprise_reporting::kBrowserReportIncludeAllProfiles)) {
    return GetReportDataForKnownProfiles(profile_manager);
  } else {
    return GetReportDataForLoadedProfiles(profile_manager);
  }
}

bool BrowserReportGeneratorIOS::IsExtendedStableChannel() {
  return false;  // Not supported on iOS.
}

void BrowserReportGeneratorIOS::GenerateBuildStateInfo(
    em::BrowserReport* report) {
  // Not used on iOS because there is no in-app auto-update.
}

}  // namespace enterprise_reporting
