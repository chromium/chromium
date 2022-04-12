// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/first_follow_coordinator.h"

#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_controller.h"
#import "ios/chrome/browser/ui/follow/first_follow_view_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

}  // namespace

@interface FirstFollowCoordinator () <FirstFollowViewDelegate>
@end

@implementation FirstFollowCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  FirstFollowViewController* firstFollowViewController =
      [[FirstFollowViewController alloc] init];
  firstFollowViewController.followedWebChannel = self.followedWebChannel;
  // Ownership is passed to VC so this object is not retained after VC closes.
  self.followedWebChannel = nil;
  firstFollowViewController.delegate = self;
  firstFollowViewController.faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  if (@available(iOS 15, *)) {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        firstFollowViewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentationController.detents =
        @[ UISheetPresentationControllerDetent.mediumDetent ];
    presentationController.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    firstFollowViewController.modalPresentationStyle =
        UIModalPresentationFormSheet;
  }

  [self.baseViewController presentViewController:firstFollowViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }
}

#pragma mark - FirstFollowViewDelegate

// Go To Feed button tapped.
- (void)handleGoToFeedTapped {
  [self.newTabPageCommandsHandler
      openNTPScrolledIntoFeedType:FeedTypeFollowing];
}

#pragma mark - Helpers

// The dispatcher used for NewTabPageCommands.
- (id<NewTabPageCommands>)newTabPageCommandsHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            NewTabPageCommands);
}

@end