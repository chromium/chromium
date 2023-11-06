// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_client_impl.h"
#import "ios/chrome/browser/search_engines/model/ui_thread_search_terms_data.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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

std::unique_ptr<KeyedService> BuildTemplateURLService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TemplateURLService>(
      browser_state->GetPrefs(),
      std::make_unique<ios::UIThreadSearchTermsData>(),
      ios::WebDataServiceFactory::GetKeywordWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<ios::TemplateURLServiceClientImpl>(
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state, ServiceAccessType::EXPLICIT_ACCESS)),
      GetDefaultSearchProviderChangedCallback());
}

}  // namespace

// static
TemplateURLService* TemplateURLServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  static base::NoDestructor<TemplateURLServiceFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
TemplateURLServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTemplateURLService);
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "TemplateURLService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

void TemplateURLServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DefaultSearchManager::RegisterProfilePrefs(registry);
  TemplateURLService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
TemplateURLServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildTemplateURLService(context);
}

web::BrowserState* TemplateURLServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool TemplateURLServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
