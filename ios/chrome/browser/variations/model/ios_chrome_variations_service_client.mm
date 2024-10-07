// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_service_client.h"

#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/variations/seed_response.h"
#import "components/variations/service/limited_entropy_synthetic_trial.h"
#import "components/variations/synthetic_trials.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

IOSChromeVariationsServiceClient::IOSChromeVariationsServiceClient() = default;

IOSChromeVariationsServiceClient::~IOSChromeVariationsServiceClient() = default;

base::Version IOSChromeVariationsServiceClient::GetVersionForSimulation() {
  // TODO(crbug.com/40816694): Get the version that will be used on restart
  // instead of the current version.
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeVariationsServiceClient::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker*
IOSChromeVariationsServiceClient::GetNetworkTimeTracker() {
  return GetApplicationContext()->GetNetworkTimeTracker();
}

bool IOSChromeVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

base::FilePath IOSChromeVariationsServiceClient::GetVariationsSeedFileDir() {
  base::FilePath seed_file_dir;
  base::PathService::Get(ios::DIR_USER_DATA, &seed_file_dir);
  return seed_file_dir;
}

std::unique_ptr<variations::SeedResponse>
IOSChromeVariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
  return [IOSChromeVariationsSeedStore popSeed];
}

bool IOSChromeVariationsServiceClient::IsEnterprise() {
  // TODO(crbug.com/40647432): Implement enterprise check for iOS.
  return false;
}

// Nothing to do, as iOS doesn't support multiple profiles.
void IOSChromeVariationsServiceClient::
    RemoveGoogleGroupsFromPrefsForDeletedProfiles(PrefService* local_state) {}

version_info::Channel IOSChromeVariationsServiceClient::GetChannel() {
  return ::GetChannel();
}
