// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/snapshot_cover_view_controller.h"

@implementation SnapshotCoverViewController {
  // The image to be shown as cover.
  UIImage* _image;
  // The action to be run when the view controller first appears.
  ProceduralBlock _onFirstAppear;
}

- (instancetype)initWithImage:(UIImage*)image
                onFirstAppear:(ProceduralBlock)onFirstAppear {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _image = image;
    _onFirstAppear = onFirstAppear;
  }
  return self;
}

- (instancetype)initWithImage:(UIImage*)image {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _image = image;
  }
  return self;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (_onFirstAppear) {
    _onFirstAppear();
    _onFirstAppear = nil;
  }
}

- (void)loadView {
  [super loadView];
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.image = _image;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:imageView];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:imageView.leadingAnchor],
    [self.view.trailingAnchor constraintEqualToAnchor:imageView.trailingAnchor],
    [self.view.topAnchor constraintEqualToAnchor:imageView.topAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:imageView.bottomAnchor]
  ]];
}

@end
