// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_profile_session_metrics_provider.h"

#import <algorithm>

#import "base/check_deref.h"
#import "base/metrics/histogram_functions.h"
#import "components/activity_reporter/activity_reporter.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace {

// Returns whether there is any session active for `profiles`.
bool IsSessionActive(const std::vector<ProfileIOS*>& profiles) {
  for (ProfileIOS* profile : profiles) {
    IOSProfileSessionDurationsService* service =
        IOSProfileSessionDurationsServiceFactory::GetForProfileIfExists(
            CHECK_DEREF(profile));
    if (service && service->IsSessionActive()) {
      return true;
    }
  }

  return false;
}

class IOSProfileSessionMetricsProvider : public metrics::MetricsProvider {
 public:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* /*uma_proto*/) override {
    const bool session_is_active = IsSessionActive(GetLoadedProfiles());
    // TODO(crbug.com/560014755): Stop reporting Session.IsActive.
    base::UmaHistogramBoolean("Session.IsActive", session_is_active);
    base::UmaHistogramBoolean("Session.IsActive2", session_is_active);
    if (session_is_active) {
      GetApplicationContext()->GetActivityReporter()->ReportActive();
    }
  }

 private:
  static std::vector<ProfileIOS*> GetLoadedProfiles() {
    return GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  }
};

}  // namespace

std::unique_ptr<metrics::MetricsProvider>
CreateIOSProfileSessionMetricsProvider() {
  return std::make_unique<IOSProfileSessionMetricsProvider>();
}
