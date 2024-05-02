// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_switching/account_switcher_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation AccountSwitcherViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

@end
