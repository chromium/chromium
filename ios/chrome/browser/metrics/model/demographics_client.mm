// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/demographics_client.h"

#import "base/check.h"
#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
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
  return SyncServiceFactory::GetForBrowserState(GetCachedBrowserState());
}

PrefService* DemographicsClient::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

PrefService* DemographicsClient::GetProfilePrefs() {
  return GetCachedBrowserState()->GetPrefs();
}

int DemographicsClient::GetNumberOfProfilesOnDisk() {
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetBrowserStateInfoCache()
      ->GetNumberOfBrowserStates();
}

// TODO(crbug.com/355629111): this API needs to be re-designed to work
// with Multiple Identities.
ChromeBrowserState* DemographicsClient::GetCachedBrowserState() {
  ChromeBrowserState* chrome_browser_state = chrome_browser_state_.get();
  if (!chrome_browser_state) {
    chrome_browser_state = GetApplicationContext()
                               ->GetChromeBrowserStateManager()
                               ->GetLastUsedBrowserStateDeprecatedDoNotUse();

    CHECK(chrome_browser_state);
    chrome_browser_state_ = chrome_browser_state->AsWeakPtr();
  }

  CHECK(chrome_browser_state);
  return chrome_browser_state;
}

}  //  namespace metrics
