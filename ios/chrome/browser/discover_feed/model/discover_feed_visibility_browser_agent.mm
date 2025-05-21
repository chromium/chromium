// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_provider_configuration.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_api.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_visibility_provider.h"

DiscoverFeedVisibilityBrowserAgent::DiscoverFeedVisibilityBrowserAgent(
    Browser* browser)
    : BrowserUserData(browser) {}

DiscoverFeedVisibilityBrowserAgent::~DiscoverFeedVisibilityBrowserAgent() {
  [visibility_provider_ shutdown];
  visibility_provider_ = nil;
}

#pragma mark - Public

bool DiscoverFeedVisibilityBrowserAgent::IsEnabled() {
  return GetVisibilityProvider().isEnabled;
}

void DiscoverFeedVisibilityBrowserAgent::SetEnabled(bool enabled) {
  GetVisibilityProvider().enabled = enabled;
}

DiscoverFeedEligibility DiscoverFeedVisibilityBrowserAgent::GetEligibility() {
  return [GetVisibilityProvider() eligibility];
}

bool DiscoverFeedVisibilityBrowserAgent::ShouldBeVisible() {
  return GetEligibility() == DiscoverFeedEligibility::kEligible && IsEnabled();
}

void DiscoverFeedVisibilityBrowserAgent::AddObserver(
    id<DiscoverFeedVisibilityObserver> observer) {
  [GetVisibilityProvider() addObserver:observer];
}

void DiscoverFeedVisibilityBrowserAgent::RemoveObserver(
    id<DiscoverFeedVisibilityObserver> observer) {
  [GetVisibilityProvider() removeObserver:observer];
}

#pragma mark - Private

id<DiscoverFeedVisibilityProvider>
DiscoverFeedVisibilityBrowserAgent::GetVisibilityProvider() {
  if (!visibility_provider_) {
    DiscoverFeedVisibilityProviderConfiguration* configuration =
        [[DiscoverFeedVisibilityProviderConfiguration alloc] init];
    ProfileIOS* profile = browser_->GetProfile();
    configuration.prefService = profile->GetPrefs();
    configuration.discoverFeedService =
        DiscoverFeedServiceFactory::GetForProfile(profile);
    configuration.templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    configuration.regionalCapabilitiesService =
        ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
    configuration.resumedFromSafeMode =
        [browser_->GetSceneState().profileState.appState resumingFromSafeMode];
    visibility_provider_ =
        ios::provider::CreateDiscoverFeedVisibilityProvider(configuration);
  }
  return visibility_provider_;
}
