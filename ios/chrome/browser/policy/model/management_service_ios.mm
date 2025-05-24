// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/management_service_ios.h"

#import "ios/chrome/browser/policy/model/management_status_providers_ios.h"

namespace policy {

namespace {

std::vector<std::unique_ptr<ManagementStatusProvider>>
GetManagementStatusProviders(ProfileIOS* profile) {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
  providers.emplace_back(
      std::make_unique<BrowserCloudManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<LocalDomainBrowserManagementStatusProvider>());
  providers.emplace_back(
      std::make_unique<ProfileCloudManagementStatusProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyUserManagementProvider>(profile));
  providers.emplace_back(
      std::make_unique<LocalTestPolicyBrowserManagementProvider>(profile));
  return providers;
}

}  // namespace

ManagementServiceIOS::ManagementServiceIOS(ProfileIOS* profile)
    : ManagementService(GetManagementStatusProviders(profile)) {}

ManagementServiceIOS::~ManagementServiceIOS() = default;

}  // namespace policy
