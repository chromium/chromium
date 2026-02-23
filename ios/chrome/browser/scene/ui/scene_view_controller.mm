// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import "base/check.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SceneViewController {
  // The app bar.
  UIViewController* _appBar;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _appContainer = [[UIView alloc] init];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  UIView* appContainer = self.appContainer;
  UIView* view = self.view;
  appContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:appContainer];
  appContainer.frame = view.bounds;
  AddSameConstraints(appContainer, view);
}

#pragma mark - Public

- (void)setAppBar:(UIViewController*)appBar {
  CHECK(!_appBar);
  _appBar = appBar;
  UIView* appBarView = appBar.view;
  appBarView.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* appBarContainer = self.view;

  [self addChildViewController:appBar];
  [appBarContainer addSubview:appBarView];

  AddSameCenterConstraints(appBarContainer, appBarView);

  [appBar didMoveToParentViewController:self];
}

@end
