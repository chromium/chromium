// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

namespace {

// The histogram name for the Lens button new badge status.
const char kNTPLensButtonNewBadgeShownHistogram[] =
    "IOS.NTP.LensButtonNewBadgeShown";

// The maximum number of times to show the new badge on the new tab page.
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
      DiscoverFeedServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  return discoverFeedService->GetFeedMetricsRecorder();
}

- (NewTabPageHeaderViewController*)headerViewControllerForBrowser:
    (Browser*)browser {
  PrefService* prefService = browser->GetBrowserState()->GetPrefs();
  NSInteger lensNewBadgeShowCount =
      prefService->GetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount);
  if (lensNewBadgeShowCount < kMaxShowCountNTPLensButtonNewBadge) {
    // Show the "New" badge and colored symbol.
    LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult::kShown);
    prefService->SetInteger(prefs::kNTPLensEntryPointNewBadgeShownCount,
                            lensNewBadgeShowCount + 1);
    return [[NewTabPageHeaderViewController alloc]
        initWithUseNewBadgeForLensButton:YES];
  } else {
    BOOL button_pressed = lensNewBadgeShowCount == INT_MAX;
    LogLensButtonNewBadgeShownHistogram(
        button_pressed ? IOSNTPNewBadgeShownResult::kNotShownButtonPressed
                       : IOSNTPNewBadgeShownResult::kNotShownLimitReached);
    return [[NewTabPageHeaderViewController alloc]
        initWithUseNewBadgeForLensButton:NO];
  }
}

- (NewTabPageMediator*)NTPMediatorForBrowser:(Browser*)browser
                    identityDiscImageUpdater:
                        (id<UserAccountImageUpdateDelegate>)imageUpdater {
  ChromeBrowserState* browserState = browser->GetBrowserState();
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForBrowserState(browserState);
  PrefService* prefService =
      ChromeBrowserState::FromBrowserState(browser->GetBrowserState())
          ->GetPrefs();
  BOOL isSafeMode = [browser->GetSceneState().appState resumingFromSafeMode];
  return [[NewTabPageMediator alloc]
      initWithTemplateURLService:templateURLService
                       URLLoader:UrlLoadingBrowserAgent::FromBrowser(browser)
                     authService:authService
                 identityManager:IdentityManagerFactory::GetForBrowserState(
                                     browserState)
           accountManagerService:ChromeAccountManagerServiceFactory::
                                     GetForBrowserState(browserState)
        identityDiscImageUpdater:imageUpdater
                     isIncognito:browserState->IsOffTheRecord()
             discoverFeedService:discoverFeedService
                     prefService:prefService
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
      DiscoverFeedServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
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
      DiscoverFeedServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
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

- (FeedHeaderViewController*)feedHeaderViewControllerWithFollowingDotVisible:
    (BOOL)followingDotVisible {
  return [[FeedHeaderViewController alloc]
      initWithFollowingDotVisible:followingDotVisible];
}

@end
