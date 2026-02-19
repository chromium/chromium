// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_profile_metrics_service_factory.h"

#import <memory>

#import "base/no_destructor.h"
#import "components/metrics/profile_metrics_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
metrics::ProfileMetricsService* IOSProfileMetricsServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<metrics::ProfileMetricsService>(
      profile, /*create=*/true);
}

// static
IOSProfileMetricsServiceFactory*
IOSProfileMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<IOSProfileMetricsServiceFactory> instance;
  return instance.get();
}

// Session time in incognito is counted towards the session time in the
// regular profile. That means that for a user that is signed in in their
// regular profile and that is browsing in incognito profile,
// Chromium will record the session time as being signed in.
IOSProfileMetricsServiceFactory::IOSProfileMetricsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("ProfileMetricsService",
                                    ProfileSelection::kRedirectedInIncognito) {}

IOSProfileMetricsServiceFactory::~IOSProfileMetricsServiceFactory() = default;

std::unique_ptr<KeyedService>
IOSProfileMetricsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // Per-profile metrics are not supported on iOS: create a service with an
  // empty profile context.
  return std::make_unique<metrics::ProfileMetricsService>(
      metrics::ProfileMetricsContext());
}
