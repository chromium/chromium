// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_signin_and_sync_status_metrics_provider.h"

#import <vector>

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
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
  signin_metrics::ProfilesStatus profiles_status;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    auto* session_duration =
        IOSProfileSessionDurationsServiceFactory::GetForProfile(profile);
    signin_metrics::UpdateProfilesStatusBasedOnSignInAndSyncStatus(
        profiles_status, session_duration->GetSigninStatus(),
        session_duration->IsSyncing());
  }
  return profiles_status;
}
