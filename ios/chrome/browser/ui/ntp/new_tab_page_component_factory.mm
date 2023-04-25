// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory.h"

#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (NewTabPageHeaderViewController*)headerViewController {
  return [[NewTabPageHeaderViewController alloc] init];
}

- (NewTabPageMediator*)NTPMediatorForBrowser:(Browser*)browser
                                    webState:(web::WebState*)webState
                    identityDiscImageUpdater:
                        (id<UserAccountImageUpdateDelegate>)imageUpdater {
  ChromeBrowserState* browserState = browser->GetBrowserState();
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  return [[NewTabPageMediator alloc]
              initWithWebState:webState
            templateURLService:templateURLService
                     URLLoader:UrlLoadingBrowserAgent::FromBrowser(browser)
                   authService:authService
               identityManager:IdentityManagerFactory::GetForBrowserState(
                                   browserState)
         accountManagerService:ChromeAccountManagerServiceFactory::
                                   GetForBrowserState(browserState)
                    logoVendor:ios::provider::CreateLogoVendor(browser,
                                                               webState)
      identityDiscImageUpdater:imageUpdater];
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

@end
