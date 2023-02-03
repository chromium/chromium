// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller_delegate.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkNavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  UIViewController<UIAdaptivePresentationControllerDelegate>*
      adaptiveViewController = base::mac::ObjCCast<
          UIViewController<UIAdaptivePresentationControllerDelegate>>(
          viewController);
  navigationController.presentationController.delegate = adaptiveViewController;
}

@end
