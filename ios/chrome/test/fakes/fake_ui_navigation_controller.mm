// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"

@implementation FakeUINavigationController {
  // View controller presented using
  // `presentViewController:animated:completion:`
  UIViewController* _presentedViewController;
  // View controllers pushed using `pushViewController:animated:completion:` or
  // set using `setViewControllers:animated:`
  NSMutableArray<UIViewController*>* _viewControllers;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _viewControllers = [NSMutableArray array];
  }
  return self;
}

#pragma mark - UIViewController

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  _presentedViewController = viewControllerToPresent;
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  _presentedViewController = nil;
}

- (UIViewController*)presentedViewController {
  return _presentedViewController;
}

#pragma mark - UINavigationController

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  [_viewControllers addObject:viewController];
}

- (UIViewController*)popViewControllerAnimated:(BOOL)animated {
  UIViewController* lastObject = _viewControllers.lastObject;
  if (lastObject) {
    [_viewControllers removeLastObject];
  }
  return lastObject;
}

- (NSArray<UIViewController*>*)popToViewController:
                                   (UIViewController*)viewController
                                          animated:(BOOL)animated {
  NSMutableArray<UIViewController*>* popped = [NSMutableArray array];
  while (self.topViewController && self.topViewController != viewController) {
    [popped addObject:[self popViewControllerAnimated:animated]];
  }
  return popped;
}

- (NSArray<UIViewController*>*)popToRootViewControllerAnimated:(BOOL)animated {
  return [self popToViewController:self.viewControllers.firstObject
                          animated:animated];
}

- (UIViewController*)topViewController {
  return _viewControllers.lastObject;
}

- (UIViewController*)visibleViewController {
  if (self.presentedViewController) {
    return self.presentedViewController;
  } else {
    return self.topViewController;
  }
}

- (NSArray<UIViewController*>*)viewControllers {
  return _viewControllers;
}

- (void)setViewControllers:(NSArray<UIViewController*>*)viewControllers
                  animated:(BOOL)animated {
  _viewControllers = [NSMutableArray arrayWithArray:viewControllers];
}

@end
