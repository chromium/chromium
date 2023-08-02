// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_signin_and_sync_status_metrics_provider.h"

#import <vector>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

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
