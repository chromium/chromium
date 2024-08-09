// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"

#import "base/metrics/histogram_functions.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/page_info/core/about_this_site_service.h"
#import "components/page_info/core/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
page_info::AboutThisSiteService*
AboutThisSiteServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<page_info::AboutThisSiteService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
AboutThisSiteServiceFactory* AboutThisSiteServiceFactory::GetInstance() {
  static base::NoDestructor<AboutThisSiteServiceFactory> instance;
  return instance.get();
}

AboutThisSiteServiceFactory::AboutThisSiteServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "AboutThisSiteServiceFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
}

AboutThisSiteServiceFactory::~AboutThisSiteServiceFactory() = default;

std::unique_ptr<KeyedService>
AboutThisSiteServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  const bool is_about_this_site_language_supported =
      page_info::IsAboutThisSiteFeatureEnabled(
          GetApplicationContext()->GetApplicationLocale());

  base::UmaHistogramBoolean("Security.PageInfo.AboutThisSiteLanguageSupported",
                            is_about_this_site_language_supported);

  if (!is_about_this_site_language_supported) {
    return nullptr;
  }

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  auto* optimization_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(browser_state);
  if (!optimization_guide) {
    return nullptr;
  }

  auto* template_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  // TemplateURLService may be null during testing.
  if (!template_service) {
    return nullptr;
  }

  return std::make_unique<page_info::AboutThisSiteService>(
      optimization_guide, browser_state->IsOffTheRecord(),
      browser_state->GetPrefs(), template_service);
}
