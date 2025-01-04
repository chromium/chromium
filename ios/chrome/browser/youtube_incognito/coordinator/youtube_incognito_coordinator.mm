// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator.h"

#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator_delegate.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"
#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet_delegate.h"

namespace {

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

}  // namespace

@interface YoutubeIncognitoCoordinator () <YoutubeIncognitoSheetDelegate>
@end

@implementation YoutubeIncognitoCoordinator {
  YoutubeIncognitoSheet* _viewController;
}

- (void)start {
  // TODO(crbug.com/374935670): Add the the case when incognito is unavailable
  // and show toast when the view was presented already.
  __weak YoutubeIncognitoCoordinator* weakSelf = self;
  [self.tabOpener
      dismissModalsAndMaybeOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                     INCOGNITO
                               withUrlLoadParams:self.urlLoadParams
                                  dismissOmnibox:YES
                                      completion:^{
                                        [weakSelf presentViewController];
                                      }];
}

- (void)stop {
  [super stop];
  [self dismissViewController];
}

#pragma mark - YoutubeIncognitoSheetDelegate

- (void)didTapPrimaryActionButton {
  CHECK(_viewController);
  [self.delegate shouldStopYoutubeIncognitoCoordinator:self];
}

#pragma mark - Private

// Dismisses the YoutubeIncognitoCoordinator's view controller.
- (void)dismissViewController {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

// Presents the YoutubeIncognitoCoordinator's view controller.
- (void)presentViewController {
  _viewController = [[YoutubeIncognitoSheet alloc] init];
  _viewController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];
  _viewController.sheetPresentationController.preferredCornerRadius =
      kHalfSheetCornerRadius;
  _viewController.sheetPresentationController
      .widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

@end
