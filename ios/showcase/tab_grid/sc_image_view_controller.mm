// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/tab_grid/sc_image_view_controller.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCImageViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor blackColor];
  UIImageView* standardImageView = [[UIImageView alloc] init];
  standardImageView.translatesAutoresizingMaskIntoConstraints = NO;
  standardImageView.contentMode = UIViewContentModeScaleAspectFill;
  standardImageView.clipsToBounds = YES;
  standardImageView.image = [UIImage imageNamed:@"Sample-screenshot-portrait"];
  [self.view addSubview:standardImageView];

  TopAlignedImageView* topAlignedImageView = [[TopAlignedImageView alloc] init];
  topAlignedImageView.translatesAutoresizingMaskIntoConstraints = NO;
  topAlignedImageView.image =
      [UIImage imageNamed:@"Sample-screenshot-portrait"];
  topAlignedImageView.clipsToBounds = YES;
  [self.view addSubview:topAlignedImageView];

  UILabel* topLabel = [[UILabel alloc] init];
  topLabel.textColor = [UIColor whiteColor];
  topLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [topLabel setText:@"Standard ImageView with aspect fill"];
  [self.view addSubview:topLabel];

  UILabel* bottomLabel = [[UILabel alloc] init];
  bottomLabel.textColor = [UIColor whiteColor];
  bottomLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [bottomLabel setText:@"TopAlignedImageView"];
  [self.view addSubview:bottomLabel];

  NSArray* constraints = @[
    [topLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [topLabel.bottomAnchor constraintEqualToAnchor:standardImageView.topAnchor],
    [standardImageView.widthAnchor constraintEqualToConstant:250.0f],
    [standardImageView.heightAnchor constraintEqualToConstant:250.0f],
    [standardImageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [standardImageView.bottomAnchor
        constraintEqualToAnchor:self.view.centerYAnchor
                       constant:-20.0f],
    [bottomLabel.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [bottomLabel.topAnchor constraintEqualToAnchor:self.view.centerYAnchor
                                          constant:20.0f],
    [topAlignedImageView.widthAnchor constraintEqualToConstant:250.0f],
    [topAlignedImageView.heightAnchor constraintEqualToConstant:250.0f],
    [topAlignedImageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [topAlignedImageView.topAnchor
        constraintEqualToAnchor:bottomLabel.bottomAnchor],

  ];
  [NSLayoutConstraint activateConstraints:constraints];
}
@end
