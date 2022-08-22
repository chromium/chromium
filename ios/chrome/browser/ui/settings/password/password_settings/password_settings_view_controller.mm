// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"

#import "ios/chrome/browser/ui/table_view/table_view_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordSettingsViewController

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

#pragma mark - UIViewController

- (void)viewWillDisappear:(BOOL)animated {
  [self.presentationDelegate passwordSettingsViewControllerDidDismiss];
}

@end
