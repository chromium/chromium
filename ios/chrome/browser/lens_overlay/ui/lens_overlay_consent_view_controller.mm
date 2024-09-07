// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kButtonHeight = 50.0f;

const NSDirectionalEdgeInsets dialogInsets = {20.0f, 20.0f, 20.0f, 20.0f};

}  // namespace

@implementation LensOverlayConsentViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/354942727): use color from mocks.
  self.view.backgroundColor = [UIColor systemBackgroundColor];

  // TODO(crbug.com/354942727): use strings from mocks and localize them.
  __weak __typeof(self) weakSelf = self;
  UIButton* acceptButton =
      [self newButtonWithTitle:@"Accept [TEST]"
                 actionHandler:^(UIAction* action) {
                   [weakSelf.delegate consentViewController:weakSelf
                                 didFinishWithTermsAccepted:YES];
                 }];

  UIButton* denyButton =
      [self newButtonWithTitle:@"Deny [TEST]"
                 actionHandler:^(UIAction* action) {
                   [weakSelf.delegate consentViewController:weakSelf
                                 didFinishWithTermsAccepted:NO];
                 }];

  NSArray<UIButton*>* buttons = @[ acceptButton, denyButton ];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = @"Lens Overlay [todo:localize]";

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text =
      @"Wanna use Lens Overlay? Please give your consent [todo:localize]";
  bodyLabel.numberOfLines = 0;

  UIView* videoView = [[UIView alloc] init];
  videoView.layer.cornerRadius = 15.0f;
  videoView.backgroundColor = [UIColor lightGrayColor];
  videoView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, videoView, bodyLabel, acceptButton, denyButton
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.spacing = 20.0f;

  [self.view addSubview:verticalStack];

  // Setup constraints

  for (UIButton* button in buttons) {
    [NSLayoutConstraint activateConstraints:@[
      [button.heightAnchor constraintGreaterThanOrEqualToConstant:kButtonHeight]
    ]];
  }

  AddSameConstraintsWithInsets(verticalStack, self.view, dialogInsets);
}

- (UIButton*)newButtonWithTitle:(NSString*)title
                  actionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.title = title;
  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];

  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;

  return button;
}

@end
