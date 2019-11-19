// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/advanced_signin_settings_navigation_controller.h"

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AdvancedSigninSettingsNavigationController

- (UIViewController*)popViewControllerAnimated:(BOOL)animated {
  UIViewController* poppedViewController =
      [super popViewControllerAnimated:animated];
  if ([poppedViewController
          respondsToSelector:@selector(viewControllerWasPopped)]) {
    [poppedViewController performSelector:@selector(viewControllerWasPopped)];
  }
  return poppedViewController;
}

@end
