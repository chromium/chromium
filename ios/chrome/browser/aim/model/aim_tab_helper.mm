// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/aim_tab_helper.h"

#import "base/feature_list.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

AimTabHelper::AimTabHelper(web::WebState* web_state) {
  observation_.Observe(web_state);
}

AimTabHelper::~AimTabHelper() = default;

#pragma mark - web::WebStateObserver

void AimTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument() ||
      !navigation_context->HasCommitted()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(omnibox::kAimUrlNavigationFetchEnabled)) {
    return;
  }

  const GURL& url = navigation_context->GetUrl();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());

  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile);

  if (!aim_eligibility_service || !aim_eligibility_service->IsAimEligible()) {
    return;
  }

  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return;
  }

  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_search_provider ||
      !default_search_provider->IsSearchURL(
          url, template_url_service->search_terms_data())) {
    return;
  }

  if (!aim_eligibility_service->HasAimUrlParams(url)) {
    return;
  }

  aim_eligibility_service->FetchEligibility(
      AimEligibilityService::RequestSource::kAimUrlNavigation);
}

void AimTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
}
