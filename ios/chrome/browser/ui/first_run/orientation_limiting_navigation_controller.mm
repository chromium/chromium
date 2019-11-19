// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OrientationLimitingNavigationController

- (NSUInteger)supportedInterfaceOrientations {
  return IsIPadIdiom() ? [super supportedInterfaceOrientations]
                       : UIInterfaceOrientationMaskPortrait;
}

- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
  return IsIPadIdiom() ? [super preferredInterfaceOrientationForPresentation]
                       : UIInterfaceOrientationPortrait;
}

- (BOOL)shouldAutorotate {
  return IsIPadIdiom() ? [super shouldAutorotate] : NO;
}

@end
