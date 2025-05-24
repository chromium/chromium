// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_coordinator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/user_account_image_update_delegate.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_factory.h"
#import "ios/chrome/browser/image_fetcher/model/image_fetcher_service_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_view_controller.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

namespace {

// The histogram name for the Lens button new badge status.
const char kNTPLensButtonNewBadgeShownHistogram[] =
    "IOS.NTP.LensButtonNewBadgeShown";

// The maximum number of times to show the new badge on the Lens entrypoint.
const NSInteger kMaxShowCountNTPLensButtonNewBadge = 3;

// Logs the Lens button new badge shown histogram.
void LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult result) {
  base::UmaHistogramEnumeration(kNTPLensButtonNewBadgeShownHistogram, result);
}

}  // namespace

@implementation NewTabPageComponentFactory

#pragma mark - NewTabPageComponentFactoryProtocol methods

- (ContentSuggestionsCoordinator*)contentSuggestionsCoordinatorForBrowser:
    (Browser*)browser {
  return [[ContentSuggestionsCoordinator alloc]
      initWithBaseViewController:nil
                         browser:browser];
}

- (FeedMetricsRecorder*)feedMetricsRecorderForBrowser:(Browser*)browser {
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForProfile(browser->GetProfile());
  return discoverFeedService->GetFeedMetricsRecorder();
}

- (NewTabPageHeaderViewController*)headerViewController {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  NSInteger lensNewBadgeShowCount =
      localState->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount);

  BOOL useNewBadgeForCustomizationMenu = NO;
  NSInteger customizationNewBadgeImpressionCount = localState->GetInteger(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount);

  if (customizationNewBadgeImpressionCount <
      kCustomizationNewBadgeMaxImpressionCount) {
    useNewBadgeForCustomizationMenu = YES;
    base::RecordAction(
        base::UserMetricsAction(kNTPCustomizationNewBadgeShownAction));
    localState->SetInteger(prefs::kNTPHomeCustomizationNewBadgeImpressionCount,
                           customizationNewBadgeImpressionCount + 1);
  }

  if (lensNewBadgeShowCount < kMaxShowCountNTPLensButtonNewBadge) {
    // Show the "New" badge and colored symbol.
    LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult::kShown);
    localState->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount,
                           lensNewBadgeShowCount + 1);
    return [[NewTabPageHeaderViewController alloc]
        initWithUseNewBadgeForLensButton:YES
         useNewBadgeForCustomizationMenu:useNewBadgeForCustomizationMenu];
  } else {
    BOOL button_pressed = lensNewBadgeShowCount == INT_MAX;
    LogLensButtonNewBadgeShownHistogram(
        button_pressed ? IOSNTPNewBadgeShownResult::kNotShownButtonPressed
                       : IOSNTPNewBadgeShownResult::kNotShownLimitReached);
    return [[NewTabPageHeaderViewController alloc]
        initWithUseNewBadgeForLensButton:NO
         useNewBadgeForCustomizationMenu:useNewBadgeForCustomizationMenu];
  }
}

- (NewTabPageMediator*)NTPMediatorForBrowser:(Browser*)browser
                    identityDiscImageUpdater:
                        (id<UserAccountImageUpdateDelegate>)imageUpdater {
  ProfileIOS* profile = browser->GetProfile();
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  UrlLoadingBrowserAgent* URLLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(browser);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  regional_capabilities::RegionalCapabilitiesService*
      regionalCapabilitiesService =
          ios::RegionalCapabilitiesServiceFactory::GetForProfile(profile);
  HomeBackgroundCustomizationService* backgroundCustomizationService =
      HomeBackgroundCustomizationServiceFactory::GetForProfile(profile);
  image_fetcher::ImageFetcherService* imageFetcherService =
      ImageFetcherServiceFactory::GetForProfile(profile);
  BrowserViewVisibilityNotifierBrowserAgent*
      browserViewVisibilityNotifierBrowserAgent =
          BrowserViewVisibilityNotifierBrowserAgent::FromBrowser(browser);
  DiscoverFeedVisibilityBrowserAgent* discoverFeedVisibilityBrowserAgent =
      DiscoverFeedVisibilityBrowserAgent::FromBrowser(browser);
  return [[NewTabPageMediator alloc]
              initWithTemplateURLService:templateURLService
                               URLLoader:URLLoadingBrowserAgent
                             authService:authService
                         identityManager:identityManager
                   accountManagerService:accountManagerService
                identityDiscImageUpdater:imageUpdater
                     discoverFeedService:discoverFeedService
                             prefService:prefService
                             syncService:syncService
             regionalCapabilitiesService:regionalCapabilitiesService
          backgroundCustomizationService:backgroundCustomizationService
                     imageFetcherService:imageFetcherService
           browserViewVisibilityNotifier:
               browserViewVisibilityNotifierBrowserAgent
      discoverFeedVisibilityBrowserAgent:discoverFeedVisibilityBrowserAgent];
}

- (NewTabPageViewController*)NTPViewController {
  return [[NewTabPageViewController alloc] init];
}

- (UIViewController*)discoverFeedForBrowser:(Browser*)browser
                viewControllerConfiguration:
                    (DiscoverFeedViewControllerConfiguration*)
                        viewControllerConfiguration {
  // Get the feed factory from the `browser` and create the feed model.
  DiscoverFeedService* feedService =
      DiscoverFeedServiceFactory::GetForProfile(browser->GetProfile());
  FeedModelConfiguration* discoverFeedConfiguration =
      [FeedModelConfiguration discoverFeedModelConfiguration];
  feedService->CreateFeedModel(discoverFeedConfiguration);

  // Return Discover feed VC created with `viewControllerConfiguration`.
  return feedService->NewDiscoverFeedViewControllerWithConfiguration(
      viewControllerConfiguration);
}

- (UIViewController*)followingFeedForBrowser:(Browser*)browser
                 viewControllerConfiguration:
                     (DiscoverFeedViewControllerConfiguration*)
                         viewControllerConfiguration
                                    sortType:(FollowingFeedSortType)sortType {
  // Get the feed factory from the `browser` and create the feed model. Content
  // is sorted by `sortType`.
  DiscoverFeedService* feedService =
      DiscoverFeedServiceFactory::GetForProfile(browser->GetProfile());
  FeedModelConfiguration* followingFeedConfiguration =
      [FeedModelConfiguration followingModelConfigurationWithSortType:sortType];
  feedService->CreateFeedModel(followingFeedConfiguration);

  // Return Following feed VC created with `viewControllerConfiguration`.
  return feedService->NewFollowingFeedViewControllerWithConfiguration(
      viewControllerConfiguration);
}

- (FeedWrapperViewController*)
    feedWrapperViewControllerWithDelegate:
        (id<FeedWrapperViewControllerDelegate>)delegate
                       feedViewController:
                           (UIViewController*)feedViewController {
  return
      [[FeedWrapperViewController alloc] initWithDelegate:delegate
                                       feedViewController:feedViewController];
}

- (FeedHeaderViewController*)feedHeaderViewController {
  return [[FeedHeaderViewController alloc] init];
}

@end
