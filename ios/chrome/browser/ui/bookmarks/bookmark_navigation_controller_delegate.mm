// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller_delegate.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_modal_presenting.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkNavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  BOOL shouldDismissOnTouchOutside = YES;

  if (IsCollectionsCardPresentationStyleEnabled()) {
    if (@available(iOS 13, *)) {
      UIViewController<UIAdaptivePresentationControllerDelegate>*
          adaptiveViewController = base::mac::ObjCCast<
              UIViewController<UIAdaptivePresentationControllerDelegate>>(
              viewController);
      navigationController.presentationController.delegate =
          adaptiveViewController;
    }
  }

  ChromeTableViewController* tableViewController =
      base::mac::ObjCCast<ChromeTableViewController>(viewController);
  if (tableViewController) {
    shouldDismissOnTouchOutside =
        [tableViewController shouldBeDismissedOnTouchOutside];
  }

  id<UIViewControllerTransitionCoordinator> transitionCoordinator = nil;
  if (animated) {
    transitionCoordinator = navigationController.transitionCoordinator;
  }

  [self.modalController
      setShouldDismissOnTouchOutside:shouldDismissOnTouchOutside
           withTransitionCoordinator:transitionCoordinator];
}

@end
