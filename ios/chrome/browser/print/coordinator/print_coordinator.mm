// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/print/coordinator/print_coordinator.h"

#import "base/notreached.h"

@implementation PrintCoordinator

#pragma mark - Public

- (void)dismissAnimated:(BOOL)animated {
  // Subclass must implement.
  NOTREACHED();
}

#pragma mark - PrintHandler

- (void)printView:(UIView*)view withTitle:(NSString*)title {
  // Subclass must implement.
  NOTREACHED();
}

- (void)printView:(UIView*)view
             withTitle:(NSString*)title
    baseViewController:(UIViewController*)baseViewController {
  // Subclass must implement.
  NOTREACHED();
}

- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController {
  // Subclass must implement.
  NOTREACHED();
}

@end
