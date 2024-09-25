// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_view_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
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

- (NewTabPageHeaderViewController*)headerViewControllerForBrowser:
    (Browser*)browser {
  PrefService* prefService = browser->GetProfile()->GetPrefs();
  NSInteger lensNewBadgeShowCount =
      prefService->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount);

  BOOL useNewBadgeForCustomizationMenu = NO;
  NSInteger customizationNewBadgeImpressionCount = prefService->GetInteger(
      prefs::kNTPHomeCustomizationNewBadgeImpressionCount);

  if (customizationNewBadgeImpressionCount <
      kCustomizationNewBadgeMaxImpressionCount) {
    useNewBadgeForCustomizationMenu = YES;
    base::RecordAction(
        base::UserMetricsAction(kNTPCustomizationNewBadgeShownAction));
    prefService->SetInteger(prefs::kNTPHomeCustomizationNewBadgeImpressionCount,
                            customizationNewBadgeImpressionCount + 1);
  }

  if (lensNewBadgeShowCount < kMaxShowCountNTPLensButtonNewBadge) {
    // Show the "New" badge and colored symbol.
    LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult::kShown);
    prefService->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount,
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
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  BOOL isSafeMode = [browser->GetSceneState().appState resumingFromSafeMode];
  return [[NewTabPageMediator alloc]
      initWithTemplateURLService:templateURLService
                       URLLoader:UrlLoadingBrowserAgent::FromBrowser(browser)
                     authService:authService
                 identityManager:IdentityManagerFactory::GetForProfile(profile)
           accountManagerService:ChromeAccountManagerServiceFactory::
                                     GetForProfile(profile)
        identityDiscImageUpdater:imageUpdater
                     isIncognito:profile->IsOffTheRecord()
             discoverFeedService:discoverFeedService
                     prefService:prefService
                     syncService:syncService
                      isSafeMode:isSafeMode];
}

- (NewTabPageViewController*)NTPViewController {
  return [[NewTabPageViewController alloc] init];
}

- (UIViewController*)discoverFeedForBrowser:(Browser*)browser
                viewControllerConfiguration:
                    (DiscoverFeedViewControllerConfiguration*)
                        viewControllerConfiguration {
  if (tests_hook::DisableDiscoverFeed()) {
    return nil;
  }

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
  if (tests_hook::DisableDiscoverFeed()) {
    return nil;
  }

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
