// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/model/domain_diversity_reporter_factory.h"

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/time/default_clock.h"
#import "build/build_config.h"
#import "components/history/metrics/domain_diversity_reporter.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

DomainDiversityReporterFactory::DomainDiversityReporterFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "DomainDiversityReporter",
          ProfileSelection::kNoInstanceInIncognito,
          ServiceCreation::kCreateWithProfile,
          TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
DomainDiversityReporterFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CHECK(!profile->IsOffTheRecord());
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(profile);

  // Only build DomainDiversityReporter service with a valid `history_service`.
  if (!history_service) {
    return nullptr;
  }

  return std::make_unique<DomainDiversityReporter>(
      history_service, profile->GetPrefs(), base::DefaultClock::GetInstance());
}

void DomainDiversityReporterFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DomainDiversityReporter::RegisterProfilePrefs(registry);
}
