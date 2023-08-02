// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"

#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/follow/followed_web_site.h"
#import "ios/chrome/browser/follow/followed_web_site_state.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "net/base/mac/url_conversions.h"

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

}  // namespace

@interface FirstFollowCoordinator () <ConfirmationAlertActionHandler>

// FaviconLoader retrieves favicons for a given page URL.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

@end

@implementation FirstFollowCoordinator {
  FollowedWebSite* _followedWebSite;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           followedWebSite:(FollowedWebSite*)followedWebSite {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _followedWebSite = followedWebSite;
  }
  return self;
}

- (void)start {
  self.feedMetricsRecorder = DiscoverFeedServiceFactory::GetForBrowserState(
                                 self.browser->GetBrowserState())
                                 ->GetFeedMetricsRecorder();

  __weak __typeof(self) weakSelf = self;
  // Most of time the page url of a followed site is a valid url, but sometimes
  // it is an invalid url with only a scheme. At this time, use the RSS url
  // instead. For a followed website, at least one url between the `webPageURL`
  // and `RSSURL` should be valid with a host.
  NSURL* followedSiteURL = _followedWebSite.webPageURL.host
                               ? _followedWebSite.webPageURL
                               : _followedWebSite.RSSURL;
  DCHECK(followedSiteURL.host);

  FirstFollowViewController* firstFollowViewController =
      [[FirstFollowViewController alloc]
          initWithTitle:_followedWebSite.title
                 active:_followedWebSite.state ==
                                FollowedWebSiteStateStateActive
                            ? YES
                            : NO
          faviconSource:^(void (^completion)(UIImage* favicon)) {
            [weakSelf faviconForPageURL:followedSiteURL completion:completion];
          }];

  firstFollowViewController.actionHandler = self;

  self.faviconLoader = IOSChromeFaviconLoaderFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  if (@available(iOS 15, *)) {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        firstFollowViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationFormSheet;
  }

  [self.baseViewController
      presentViewController:firstFollowViewController
                   animated:YES
                 completion:^() {
                   [weakSelf.feedMetricsRecorder recordFirstFollowShown];
                 }];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.feedMetricsRecorder recordFirstFollowTappedGoToFeed];
  if (self.baseViewController.presentedViewController) {
    __weak __typeof(self) weakSelf = self;
    [self.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf openNTPToFollowIfFeedAvailable];
                           }];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.feedMetricsRecorder recordFirstFollowTappedGotIt];
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
}

#pragma mark - Helpers

- (void)faviconForPageURL:(NSURL*)URL
               completion:(void (^)(UIImage*))completion {
  self.faviconLoader->FaviconForPageUrl(
      net::GURLWithNSURL(URL), kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        completion(attributes.faviconImage);
      });
}

// The dispatcher used for NewTabPageCommands.
- (id<NewTabPageCommands>)newTabPageCommandsHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            NewTabPageCommands);
}

- (void)openNTPToFollowIfFeedAvailable {
  if (_followedWebSite.state == FollowedWebSiteStateStateActive) {
    [self.newTabPageCommandsHandler
        openNTPScrolledIntoFeedType:FeedTypeFollowing];
  }
}

@end
