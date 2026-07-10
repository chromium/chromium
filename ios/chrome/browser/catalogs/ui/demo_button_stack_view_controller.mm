// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/demo_button_stack_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation DemoButtonStackViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UILabel* contentViewExample = [[UILabel alloc] init];
  contentViewExample.text = @"This is a scrollable content view";
  contentViewExample.font = [UIFont systemFontOfSize:24];
  contentViewExample.translatesAutoresizingMaskIntoConstraints = NO;

  [self.contentView addSubview:contentViewExample];

  AddSameCenterConstraints(self.contentView, contentViewExample);
}

@end
