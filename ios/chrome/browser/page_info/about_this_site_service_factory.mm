// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"

#import "base/metrics/histogram_functions.h"
#import "components/page_info/core/about_this_site_service.h"
#import "components/page_info/core/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
page_info::AboutThisSiteService* AboutThisSiteServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<page_info::AboutThisSiteService>(
      profile, /*create=*/true);
}

// static
AboutThisSiteServiceFactory* AboutThisSiteServiceFactory::GetInstance() {
  static base::NoDestructor<AboutThisSiteServiceFactory> instance;
  return instance.get();
}

AboutThisSiteServiceFactory::AboutThisSiteServiceFactory()
    : ProfileKeyedServiceFactoryIOS("AboutThisSiteServiceFactory",
                                    ServiceCreation::kCreateWithProfile) {
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

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  auto* optimization_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  if (!optimization_guide) {
    return nullptr;
  }

  auto* template_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  // TemplateURLService may be null during testing.
  if (!template_service) {
    return nullptr;
  }

  return std::make_unique<page_info::AboutThisSiteService>(
      optimization_guide, profile->IsOffTheRecord(), profile->GetPrefs(),
      template_service);
}
