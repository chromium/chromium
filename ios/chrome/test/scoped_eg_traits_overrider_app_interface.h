// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_APP_INTERFACE_H_
#define IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// App-side helpers of `ScopedTraitOverrider`. The trait overrides are read and
// written in the app process, so that UIKit gets the app's trait classes.
@interface ScopedTraitOverriderAppInterface : NSObject

// Returns the preferred content size category override of `viewController`, or
// `nil` if it has none. Unlike the trait collection of `viewController`, this
// doesn't include the value inherited from the environment.
+ (UIContentSizeCategory)contentSizeCategoryOverrideForViewController:
    (UIViewController*)viewController;

// Overrides the preferred content size category of `viewController` with
// `category`. If `category` is `nil`, removes the override instead, so that
// `viewController` inherits the trait from its environment again.
+ (void)setContentSizeCategoryOverride:(UIContentSizeCategory)category
                     forViewController:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_APP_INTERFACE_H_
