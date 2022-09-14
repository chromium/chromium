// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_transitioning_delegate.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkTransitioningDelegate
@synthesize presentationControllerModalDelegate =
    _presentationControllerModalDelegate;

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  TableViewPresentationController* controller =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  controller.modalDelegate = self.presentationControllerModalDelegate;
  return controller;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  if (self.presentationControllerModalDelegate) {
    TableViewPresentationController* controller =
        base::mac::ObjCCast<TableViewPresentationController>(
            presented.presentationController);
    // If the expected presentation cannot be dismissed by a touch outside the
    // table view, then use the default UIKit transition.
    if (controller &&
        ![self.presentationControllerModalDelegate
            presentationControllerShouldDismissOnTouchOutside:controller]) {
      return nil;
    }
  }

  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  if (self.presentationControllerModalDelegate) {
    TableViewPresentationController* controller =
        base::mac::ObjCCast<TableViewPresentationController>(
            dismissed.presentationController);
    // If the current presentation cannot be dismissed by a touch outside the
    // table view, then use the default UIKit transition.
    if (controller &&
        ![self.presentationControllerModalDelegate
            presentationControllerShouldDismissOnTouchOutside:controller]) {
      return nil;
    }
  }

  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = NO;
  return animator;
}

@end
