// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/start_suggest_service_factory.h"

#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

// static
StartSuggestService* StartSuggestServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state,
    bool create_if_necessary) {
  return static_cast<StartSuggestService*>(
      GetInstance()->GetServiceForBrowserState(browser_state,
                                               create_if_necessary));
}

// static
StartSuggestServiceFactory* StartSuggestServiceFactory::GetInstance() {
  static base::NoDestructor<StartSuggestServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
StartSuggestServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  auto url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  return std::make_unique<StartSuggestService>(
      template_url_service, url_loader_factory,
      std::make_unique<AutocompleteSchemeClassifierImpl>(),
      GetApplicationContext()->GetApplicationCountry(),
      GetApplicationContext()->GetApplicationLocale(),
      GURL(kChromeUINewTabURL));
}

StartSuggestServiceFactory::StartSuggestServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "StartSuggestService",
          BrowserStateDependencyManager::GetInstance()) {}

StartSuggestServiceFactory::~StartSuggestServiceFactory() {}
