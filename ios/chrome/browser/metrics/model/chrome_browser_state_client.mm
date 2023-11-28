// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/chrome_browser_state_client.h"

#import "base/time/time.h"
#import "components/network_time/network_time_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

namespace metrics {

ChromeBrowserStateClient::~ChromeBrowserStateClient() {}

base::Time ChromeBrowserStateClient::GetNetworkTime() const {
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

syncer::SyncService* ChromeBrowserStateClient::GetSyncService() {
  // Get SyncService from BrowserState that was the last to be used. Will create
  // a new BrowserState if no BrowserState exists.
  return SyncServiceFactory::GetForBrowserState(
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLastUsedBrowserState()
          ->GetOriginalChromeBrowserState());
}

PrefService* ChromeBrowserStateClient::GetLocalState() {
  return GetApplicationContext()->GetLocalState();
}

PrefService* ChromeBrowserStateClient::GetProfilePrefs() {
  // Get PrefService from BrowserState that was the last to be used. Will create
  // a new BrowserState if no BrowserState exists.
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetLastUsedBrowserState()
      ->GetOriginalChromeBrowserState()
      ->GetPrefs();
}

int ChromeBrowserStateClient::GetNumberOfProfilesOnDisk() {
  // Return 1 because there should be only one Profile available.
  return GetApplicationContext()
      ->GetChromeBrowserStateManager()
      ->GetBrowserStateInfoCache()
      ->GetNumberOfBrowserStates();
}

}  //  namespace metrics
