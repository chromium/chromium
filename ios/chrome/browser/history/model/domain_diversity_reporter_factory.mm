// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/model/domain_diversity_reporter_factory.h"

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "build/build_config.h"
#import "components/history/metrics/domain_diversity_reporter.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
DomainDiversityReporter* DomainDiversityReporterFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<DomainDiversityReporter*>(
      GetInstance()->GetServiceForBrowserState(profile, /*create=*/true));
}

// static
DomainDiversityReporterFactory* DomainDiversityReporterFactory::GetInstance() {
  static base::NoDestructor<DomainDiversityReporterFactory> instance;
  return instance.get();
}

DomainDiversityReporterFactory::DomainDiversityReporterFactory()
    : BrowserStateKeyedServiceFactory(
          "DomainDiversityReporter",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

DomainDiversityReporterFactory::~DomainDiversityReporterFactory() = default;

std::unique_ptr<KeyedService>
DomainDiversityReporterFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  if (profile->IsOffTheRecord()) {
    return nullptr;
  }

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  // Only build DomainDiversityReporter service with a valid `history_service`.
  if (!history_service)
    return nullptr;

  return std::make_unique<DomainDiversityReporter>(
      history_service, profile->GetPrefs(), base::DefaultClock::GetInstance());
}

void DomainDiversityReporterFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DomainDiversityReporter::RegisterProfilePrefs(registry);
}

web::BrowserState* DomainDiversityReporterFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool DomainDiversityReporterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool DomainDiversityReporterFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}
