// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewNavigationController
@synthesize tableViewController = _tableViewController;

#pragma mark - Public Interface

- (instancetype)initWithTable:(ChromeTableViewController*)table {
  self = [super initWithRootViewController:table];
  if (self) {
    _tableViewController = table;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  self.navigationBar.translucent = NO;
  self.navigationBar.barTintColor = UIColor.cr_systemBackgroundColor;
  self.view.backgroundColor = UIColor.cr_systemBackgroundColor;
  self.navigationBar.prefersLargeTitles = YES;

  self.toolbar.translucent = NO;
  self.toolbar.barTintColor = UIColor.cr_systemBackgroundColor;
}

@end
