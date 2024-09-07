// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/demographics_client.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace metrics {

DemographicsClient::DemographicsClient() = default;

DemographicsClient::~DemographicsClient() = default;

base::Time DemographicsClient::GetNetworkTime() const {
  base::Time time;
  if (GetApplicationContext()->GetNetworkTimeTracker()->GetNetworkTime(
          &time, nullptr) !=
      network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE) {
    // Return null time to indicate that it could not get the network time. It
    // is the responsibility of the client to have the strategy to deal with the
    // absence of network time.
    return base::Time();
  }
  return time;
}

syncer::SyncService* DemographicsClient::GetSyncService() {
  CHECK_EQ(GetNumberOfProfilesOnDisk(), 1);
  if (ChromeBrowserState* cached_browser_state = GetCachedBrowserState()) {
    return SyncServiceFactory::GetForBrowserState(cached_browser_state);
  }

  return nullptr;
}

PrefService* DemographicsClient::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

PrefService* DemographicsClient::GetProfilePrefs() {
  CHECK_EQ(GetNumberOfProfilesOnDisk(), 1);
  if (ChromeBrowserState* cached_browser_state = GetCachedBrowserState()) {
    return cached_browser_state->GetPrefs();
  }

  return nullptr;
}

int DemographicsClient::GetNumberOfProfilesOnDisk() {
  return GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->GetNumberOfProfiles();
}

// Note: this method is only called when GetNumberOfProfilesOnDisk() == 1 thus
// the value should be stable across a single execution of the application. The
// reason is that ProfileManagerIOS only delete the data for a Profile during
// the application startup, so GetNumberOfProfilesOnDisk() can never decrease
// without restarting the application, and thus the return value cannot change
// (since the method must not be called if GetNumberOfProfilesOnDisk() > 1).
ChromeBrowserState* DemographicsClient::GetCachedBrowserState() {
  CHECK_EQ(GetNumberOfProfilesOnDisk(), 1);
  if (ChromeBrowserState* cached_browser_state = chrome_browser_state_.get()) {
    return cached_browser_state;
  }

  const std::vector<ChromeBrowserState*> loaded_profiles =
      GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();

  // Even if there is only one Profile on disk, it may have not been loaded yet.
  if (loaded_profiles.empty()) {
    return nullptr;
  }

  CHECK_EQ(loaded_profiles.size(), 1u);
  ChromeBrowserState* cached_browser_state = loaded_profiles.back();
  chrome_browser_state_ = cached_browser_state->AsWeakPtr();
  return cached_browser_state;
}

}  //  namespace metrics
