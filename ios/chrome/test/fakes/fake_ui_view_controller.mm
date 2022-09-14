// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_ui_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeUIViewController
@synthesize presentedViewController = _presentedViewController;

#pragma mark - UIViewController methods

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  self.presentedViewController = viewControllerToPresent;
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  self.presentedViewController = nil;
}

@end
