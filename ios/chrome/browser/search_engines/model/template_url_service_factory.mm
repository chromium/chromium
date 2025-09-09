// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"

#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/search_engine_choice_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_prepopulate_data_resolver_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_client_impl.h"
#import "ios/chrome/browser/search_engines/model/ui_thread_search_terms_data.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_RLZ)
#import "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ios {
namespace {

base::RepeatingClosure GetDefaultSearchProviderChangedCallback() {
#if BUILDFLAG(ENABLE_RLZ)
  return base::BindRepeating(
      base::IgnoreResult(&rlz::RLZTracker::RecordProductEvent), rlz_lib::CHROME,
      rlz::RLZTracker::ChromeOmnibox(), rlz_lib::SET_TO_GOOGLE);
#else
  return base::RepeatingClosure();
#endif
}

std::unique_ptr<KeyedService> BuildTemplateURLService(ProfileIOS* profile) {
  return std::make_unique<TemplateURLService>(
      CHECK_DEREF(profile->GetPrefs()),
      CHECK_DEREF(
          ios::SearchEngineChoiceServiceFactory::GetForProfile(profile)),
      CHECK_DEREF(ios::TemplateURLPrepopulateDataResolverFactory::GetForProfile(
          profile)),
      std::make_unique<ios::UIThreadSearchTermsData>(),
      ios::WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<ios::TemplateURLServiceClientImpl>(
          ios::HistoryServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS)),
      GetDefaultSearchProviderChangedCallback());
}

}  // namespace

// static
TemplateURLService* TemplateURLServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<TemplateURLService>(
      profile, /*create=*/true);
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  static base::NoDestructor<TemplateURLServiceFactory> instance;
  return instance.get();
}

// static
TemplateURLServiceFactory::TestingFactory
TemplateURLServiceFactory::GetDefaultFactory() {
  return base::BindOnce(&BuildTemplateURLService);
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : ProfileKeyedServiceFactoryIOS("TemplateURLService",
                                    ProfileSelection::kRedirectedInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(ios::SearchEngineChoiceServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLPrepopulateDataResolverFactory::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

void TemplateURLServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DefaultSearchManager::RegisterProfilePrefs(registry);
  TemplateURLService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
TemplateURLServiceFactory::BuildServiceInstanceFor(ProfileIOS* profile) const {
  return BuildTemplateURLService(profile);
}

}  // namespace ios
