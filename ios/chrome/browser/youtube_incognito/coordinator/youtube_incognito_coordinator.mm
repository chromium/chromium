// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator.h"

#import "ios/chrome/browser/youtube_incognito/ui/youtube_incognito_sheet.h"

namespace {

// Custom radius for the half sheet presentation.
CGFloat const kHalfSheetCornerRadius = 20;

}  // namespace

@implementation YoutubeIncognitoCoordinator {
  YoutubeIncognitoSheet* _viewController;
}

- (void)start {
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

- (void)stop {
  [super stop];
  [self dismissViewController];
}

#pragma mark - Private

// Dismisses the YoutubeIncognitoCoordinator's view controller.
- (void)dismissViewController {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

@end
