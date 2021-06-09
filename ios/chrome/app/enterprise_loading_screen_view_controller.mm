// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1178818): implement final UI.
@implementation EnterpriseLoadScreenViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = UIColor.whiteColor;

  UIImage* logo = [UIImage imageNamed:@"launchscreen_app_logo"];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:logo];
  imageView.contentMode = UIViewContentModeScaleAspectFit;

  UILabel* label = [[UILabel alloc] init];
  label.text = @"[Test string] Loading enterprise policies...";
  label.adjustsFontSizeToFitWidth = YES;

  UIActivityIndicatorView* spinner = [[UIActivityIndicatorView alloc] init];
  [spinner startAnimating];

  UIStackView* statusStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[ spinner, label ]];
  statusStackView.axis = UILayoutConstraintAxisHorizontal;
  statusStackView.translatesAutoresizingMaskIntoConstraints = NO;
  statusStackView.distribution = UIStackViewDistributionEqualSpacing;
  statusStackView.spacing = 7;

  UIStackView* mainStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, statusStackView ]];
  mainStackView.axis = UILayoutConstraintAxisVertical;
  mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
  mainStackView.distribution = UIStackViewDistributionEqualSpacing;

  [self.view addSubview:mainStackView];
  AddSameConstraintsWithInsets(
      mainStackView, self.view,
      ChromeDirectionalEdgeInsetsMake(100, 20, 100, 20));
}

@end
