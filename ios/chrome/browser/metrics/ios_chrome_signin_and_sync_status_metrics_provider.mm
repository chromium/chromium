// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_signin_and_sync_status_metrics_provider.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#include "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSigninAndSyncStatusMetricsProvider::
    IOSChromeSigninAndSyncStatusMetricsProvider() = default;
IOSChromeSigninAndSyncStatusMetricsProvider::
    ~IOSChromeSigninAndSyncStatusMetricsProvider() = default;

void IOSChromeSigninAndSyncStatusMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  signin_metrics::EmitHistograms(GetStatusOfAllProfiles());
}

signin_metrics::ProfilesStatus
IOSChromeSigninAndSyncStatusMetricsProvider::GetStatusOfAllProfiles() const {
  std::vector<ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  signin_metrics::ProfilesStatus profiles_status;
  for (ChromeBrowserState* browser_state : browser_state_list) {
    auto* session_duration =
        IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
            browser_state);
    signin_metrics::UpdateProfilesStatusBasedOnSignInAndSyncStatus(
        profiles_status, session_duration->IsSignedIn(),
        session_duration->IsSyncing());
  }
  return profiles_status;
}
