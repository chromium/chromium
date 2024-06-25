// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"

@implementation LensOverlayContainerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];

  if (!self.selectionViewController) {
    return;
  }
  [self addChildViewController:self.selectionViewController];
  [self.view addSubview:self.selectionViewController.view];

  self.selectionViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.selectionViewController.view.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:80.0f],
    [self.selectionViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
                       constant:-80.0f],
    [self.selectionViewController.view.leftAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leftAnchor],
    [self.selectionViewController.view.rightAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.rightAnchor],
  ]];

  [self.selectionViewController didMoveToParentViewController:self];
}

@end
