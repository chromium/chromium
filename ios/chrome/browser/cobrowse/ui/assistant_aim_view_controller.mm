// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kContentMargin = 16.0;
constexpr CGFloat kTitleVerticalMargin = 12.0;
constexpr CGFloat kCloseButtonSymbolPointSize = 17.0;

}  // namespace

@interface AssistantAIMViewController () <UITextFieldDelegate>
@end

@implementation AssistantAIMViewController {
  UILabel* _titleLabel;
  UIView* _webStateView;
  NSArray<NSLayoutConstraint*>* _webStateViewConstraints;
  UITextField* _temporaryTextField;
}

@synthesize mutator = _mutator;
@synthesize delegate = _delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpHeader];
  [self setUpWebStateView];
  [self setUpTemporaryTextField];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  [self.mutator
      assistantAIMViewControllerDidRequestSearchWithText:textField.text];
  return YES;
}

#pragma mark - AssistantAIMConsumer

- (void)setWebStateView:(UIView*)webStateView {
  if (_webStateView == webStateView) {
    return;
  }
  [_webStateView removeFromSuperview];
  _webStateView = webStateView;
  [self setUpWebStateView];
}

#pragma mark - Private helpers

// Sets up the web state view.
- (void)setUpWebStateView {
  if (!_webStateView || !self.isViewLoaded) {
    return;
  }

  if (_webStateViewConstraints) {
    [NSLayoutConstraint deactivateConstraints:_webStateViewConstraints];
    _webStateViewConstraints = nil;
  }

  _webStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_webStateView atIndex:0];

  _webStateViewConstraints = @[
    [_webStateView.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                            constant:kTitleVerticalMargin],
    [_webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_webStateView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:_webStateViewConstraints];
}

// Sets up the title.
- (void)setUpHeader {
  // Close Button.
  UIButton* closeButton = [self createCloseButton];
  [self.view addSubview:closeButton];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.text = @"AI Assistant";
  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:kTitleVerticalMargin],
    [closeButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                               constant:-kContentMargin],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:closeButton.leadingAnchor
                                 constant:-kContentMargin],
  ]];
  AddSameCenterXConstraint(_titleLabel, self.view);
  AddSameCenterYConstraint(closeButton, _titleLabel);
}

// Creates and configures the close button.
- (UIButton*)createCloseButton {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.image = DefaultSymbolTemplateWithPointSize(
      kXMarkSymbol, kCloseButtonSymbolPointSize);
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  buttonConfiguration.background.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  ExtendedTouchTargetButton* closeButton =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
  [closeButton addTarget:self
                  action:@selector(didTapCloseButton)
        forControlEvents:UIControlEventTouchUpInside];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  return closeButton;
}

// Called when the close button is tapped.
- (void)didTapCloseButton {
  [self.delegate assistantAIMViewControllerDidTapClose:self];
}

- (void)setUpTemporaryTextField {
  _temporaryTextField = [[UITextField alloc] init];
  _temporaryTextField.delegate = self;
  _temporaryTextField.returnKeyType = UIReturnKeySearch;
}

@end
