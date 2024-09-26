// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_profile_session_metrics_provider.h"

#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"

namespace {

class IOSProfileSessionMetricsProvider : public metrics::MetricsProvider {
 public:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* /*uma_proto*/) override {
    const bool session_is_active = base::ranges::any_of(
        GetLoadedProfiles(), &IOSProfileSessionMetricsProvider::IsSessionActive,
        &IOSProfileSessionDurationsServiceFactory::GetForBrowserState);
    base::UmaHistogramBoolean("Session.IsActive", session_is_active);
  }

 private:
  static std::vector<ProfileIOS*> GetLoadedProfiles() {
    return GetApplicationContext()->GetProfileManager()->GetLoadedProfiles();
  }

  static bool IsSessionActive(IOSProfileSessionDurationsService* service) {
    return service->IsSessionActive();
  }
};

}  // namespace

std::unique_ptr<metrics::MetricsProvider>
CreateIOSProfileSessionMetricsProvider() {
  return std::make_unique<IOSProfileSessionMetricsProvider>();
}
