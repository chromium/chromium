// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/chrome_browser_state_client.h"

#include "components/network_time/network_time_tracker.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  return ProfileSyncServiceFactory::GetForBrowserState(
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLastUsedBrowserState()
          ->GetOriginalChromeBrowserState());
}

int ChromeBrowserStateClient::GetNumberOfProfilesOnDisk() {
  // Return 1 because there should be only one Profile available.
  return 1;
}

}  //  namespace metrics
