// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/template_url_fetcher_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/search_engines/template_url_fetcher.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"

namespace ios {

// static
TemplateURLFetcher* TemplateURLFetcherFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<TemplateURLFetcher*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
TemplateURLFetcherFactory* TemplateURLFetcherFactory::GetInstance() {
  static base::NoDestructor<TemplateURLFetcherFactory> instance;
  return instance.get();
}

TemplateURLFetcherFactory::TemplateURLFetcherFactory()
    : BrowserStateKeyedServiceFactory(
          "TemplateURLFetcher",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

TemplateURLFetcherFactory::~TemplateURLFetcherFactory() {}

std::unique_ptr<KeyedService>
TemplateURLFetcherFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return std::make_unique<TemplateURLFetcher>(
      TemplateURLServiceFactory::GetForBrowserState(
          static_cast<ChromeBrowserState*>(context)));
}

web::BrowserState* TemplateURLFetcherFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

}  // namespace ios
