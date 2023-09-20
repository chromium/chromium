// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_UI_NAVIGATION_CONTROLLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_UI_NAVIGATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Fake UINavigationController which keeps track of pushed/pop view controllers
// with a mutable array. It can also be used as a fake UIViewController i.e. it
// supports presenting or dismissing a view controller.
@interface FakeUINavigationController : UINavigationController

// List of supported UIViewController operations
// - (void)presentViewController:animated:completion:
// - (void)dismissViewControllerAnimated:completion:
// - (UIViewController*)presentedViewController

// List of supported UINavigationController operations
// - (void)pushViewController:animated:
// - (UIViewController*)popViewControllerAnimated:
// - (NSArray<UIViewController*>*)popToViewController:animated:
// - (NSArray<UIViewController*>*)popToRootViewControllerAnimated:
// - (UIViewController*)topViewController
// - (UIViewController*)visibleViewController
// - (NSArray<UIViewController*>*)viewControllers
// - (void)setViewControllers:animated:

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_UI_NAVIGATION_CONTROLLER_H_
