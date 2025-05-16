// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_app_interface.h"

#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/ui_bundled/test_infobar_delegate.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"

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
  if (@available(iOS 17, *)) {
    for (UIViewController* child in viewController.childViewControllers) {
      child.traitOverrides.horizontalSizeClass =
          UIUserInterfaceSizeClassCompact;
    }

    return [UITraitCollection
        traitCollectionWithTraits:^(id<UIMutableTraits> mutableTraits) {
          mutableTraits.horizontalSizeClass = UIUserInterfaceSizeClassCompact;
        }];
    ;
  }
#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
  else {
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
#else
  return nil;
#endif
}

@end
