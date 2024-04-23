// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/demographics_client.h"

#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace metrics {

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

ChromeBrowserState* DemographicsClient::GetCachedBrowserState() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  // If chrome_browser_state_ is defined, check it is still valid.
  if (chrome_browser_state_) {
    for (ChromeBrowserState* browser_state : browser_states) {
      // TODO(crbug.com/336468571): Replace GetDebugName() with
      // GetBrowserStateID().
      if (browser_state->GetDebugName() ==
          chrome_browser_state_->GetDebugName()) {
        return chrome_browser_state_;
      }
    }
  }

  chrome_browser_state_ = GetApplicationContext()
                              ->GetChromeBrowserStateManager()
                              ->GetLastUsedBrowserStateDeprecatedDoNotUse();
  return chrome_browser_state_;
}

}  //  namespace metrics
