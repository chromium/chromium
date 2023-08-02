// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/settings/sc_settings_coordinator.h"

@implementation SCSettingsCoordinator

@synthesize baseViewController;

- (void)start {
  UIViewController* settings = [[UIViewController alloc] init];
  settings.title = @"Settings";
  settings.view.backgroundColor = [UIColor grayColor];
  [self.baseViewController pushViewController:settings animated:YES];
}

@end
