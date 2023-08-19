// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_toolbars_view_controller.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_bottom_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"

@implementation SCToolbarsViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];

  TabGridTopToolbar* topToolbar = [[TabGridTopToolbar alloc] init];
  topToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:topToolbar];

  TabGridBottomToolbar* bottomToolbar = [[TabGridBottomToolbar alloc] init];
  bottomToolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:bottomToolbar];

  NSArray* constraints = @[
    [topToolbar.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                         constant:10.0f],
    [topToolbar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [topToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [topToolbar.heightAnchor
        constraintEqualToConstant:topToolbar.intrinsicContentSize.height],
    [bottomToolbar.topAnchor constraintEqualToAnchor:topToolbar.bottomAnchor
                                            constant:10.0f],
    [bottomToolbar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [bottomToolbar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [bottomToolbar.heightAnchor
        constraintEqualToConstant:bottomToolbar.intrinsicContentSize.height],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}
@end
