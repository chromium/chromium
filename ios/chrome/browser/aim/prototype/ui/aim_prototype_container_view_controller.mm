// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_container_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AIMPrototypeContainerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
}

- (void)addInputViewController:(UIViewController*)inputViewController {
  // TODO(crbug.com/445096991): correctly size and position the input.
  [self addChildViewController:inputViewController];
  [self.view addSubview:inputViewController.view];
  inputViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(inputViewController.view, self.view);
  [inputViewController didMoveToParentViewController:self];
}

@end
