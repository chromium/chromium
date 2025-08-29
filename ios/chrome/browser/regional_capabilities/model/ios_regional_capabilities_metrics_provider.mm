// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/regional_capabilities/model/ios_regional_capabilities_metrics_provider.h"

#import "components/regional_capabilities/program_settings.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace regional_capabilities {

IOSRegionalCapabilitiesMetricsProvider::
    IOSRegionalCapabilitiesMetricsProvider() = default;
IOSRegionalCapabilitiesMetricsProvider::
    ~IOSRegionalCapabilitiesMetricsProvider() = default;

void IOSRegionalCapabilitiesMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  absl::flat_hash_set<ActiveRegionalProgram> programs;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    RegionalCapabilitiesService* regional_capabilities =
        ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
    programs.insert(regional_capabilities->GetActiveProgramForMetrics());
  }

  RecordActiveRegionalProgram(programs);
}

}  // namespace regional_capabilities
