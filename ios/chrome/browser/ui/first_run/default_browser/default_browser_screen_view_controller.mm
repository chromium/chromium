// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/default_browser_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation DefaultBrowserScreenViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerImage = [UIImage imageNamed:@"default_browser_screen_banner"];
  self.titleText = @"WIP Default browser screen title";
  self.subtitleText = @"WIP Default browser screen subtitle";

  self.primaryActionString = @"WIP Default browser screen primary";
  self.secondaryActionString = @"WIP Default browser screen secondary";

  [super viewDidLoad];
}

@end
