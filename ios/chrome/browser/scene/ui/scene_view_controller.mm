// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/ui/scene_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SceneViewController

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
  self.appContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.appContainer];
  AddSameConstraints(self.appContainer, self.view);
}

@end
