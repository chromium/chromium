// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/template_url_service_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/google/google_url_tracker_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_client_impl.h"
#include "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

namespace ios {
namespace {

base::Closure GetDefaultSearchProviderChangedCallback() {
#if BUILDFLAG(ENABLE_RLZ)
  return base::Bind(base::IgnoreResult(&rlz::RLZTracker::RecordProductEvent),
                    rlz_lib::CHROME, rlz::RLZTracker::ChromeOmnibox(),
                    rlz_lib::SET_TO_GOOGLE);
#else
  return base::Closure();
#endif
}

std::unique_ptr<KeyedService> BuildTemplateURLService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<TemplateURLService>(
      browser_state->GetPrefs(),
      base::WrapUnique(new ios::UIThreadSearchTermsData(browser_state)),
      ios::WebDataServiceFactory::GetKeywordWebDataForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      base::WrapUnique(new ios::TemplateURLServiceClientImpl(
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state, ServiceAccessType::EXPLICIT_ACCESS))),
      ios::GoogleURLTrackerFactory::GetForBrowserState(browser_state),
      GetApplicationContext()->GetRapporServiceImpl(),
      GetDefaultSearchProviderChangedCallback());
}

}  // namespace

// static
TemplateURLService* TemplateURLServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  return base::Singleton<TemplateURLServiceFactory>::get();
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
  DependsOn(ios::GoogleURLTrackerFactory::GetInstance());
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
