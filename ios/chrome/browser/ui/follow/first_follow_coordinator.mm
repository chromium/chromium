// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"

#include "ios/chrome/browser/discover_feed/discover_feed_service.h"
#include "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/follow/first_follow_favicon_data_source.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"
#import "ios/chrome/browser/ui/follow/followed_web_channel.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

}  // namespace

@interface FirstFollowCoordinator () <ConfirmationAlertActionHandler,
                                      FirstFollowFaviconDataSource>
// FaviconLoader retrieves favicons for a given page URL.
@property(nonatomic, assign) FaviconLoader* faviconLoader;

// The view controller is owned by the view hierarchy.
@property(nonatomic, weak) FirstFollowViewController* firstFollowViewController;

// Feed metrics recorder.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

@end

@implementation FirstFollowCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  FirstFollowViewController* firstFollowViewController =
      [[FirstFollowViewController alloc] init];
  firstFollowViewController.followedWebChannel = self.followedWebChannel;
  self.feedMetricsRecorder = DiscoverFeedServiceFactory::GetForBrowserState(
                                 self.browser->GetBrowserState())
                                 ->GetFeedMetricsRecorder();
  // Ownership is passed to VC so this object is not retained after VC closes.
  self.followedWebChannel = nil;
  self.firstFollowViewController = firstFollowViewController;
  firstFollowViewController.actionHandler = self;
  firstFollowViewController.faviconDataSource = self;

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

  __weak __typeof(self) weakSelf = self;
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
    [self.baseViewController
        dismissViewControllerAnimated:YES
                           completion:^{
                             if (self.firstFollowViewController
                                     .followedWebChannel.available) {
                               [self.newTabPageCommandsHandler
                                   openNTPScrolledIntoFeedType:
                                       FeedTypeFollowing];
                             }
                           }];
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.feedMetricsRecorder recordFirstFollowTappedGotIt];
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }
}

#pragma mark - FirstFollowFaviconDataSource

- (void)faviconForURL:(CrURL*)URL
           completion:(void (^)(FaviconAttributes*))completion {
  self.faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

#pragma mark - Helpers

// The dispatcher used for NewTabPageCommands.
- (id<NewTabPageCommands>)newTabPageCommandsHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            NewTabPageCommands);
}

@end
