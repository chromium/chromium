// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/adaptive_toolbar_app_interface.h"

#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AdaptiveToolbarAppInterface

+ (BOOL)addInfobarWithTitle:(NSString*)title {
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(chrome_test_util::GetCurrentWebState());
  TestInfoBarDelegate* test_infobar_delegate = new TestInfoBarDelegate(title);
  return test_infobar_delegate->Create(manager);
}

+ (UITraitCollection*)changeTraitCollection:(UITraitCollection*)traitCollection
                          forViewController:(UIViewController*)viewController {
  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection = nil;
  // Simulate a multitasking by overriding the trait collections of the view
  // controllers.
  UITraitCollection* horizontalCompact = [UITraitCollection
      traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassCompact];
  secondTraitCollection =
      [UITraitCollection traitCollectionWithTraitsFromCollections:@[
        traitCollection, horizontalCompact
      ]];
  for (UIViewController* child in viewController.childViewControllers) {
    [viewController setOverrideTraitCollection:secondTraitCollection
                        forChildViewController:child];
  }
  return secondTraitCollection;
}

@end
