// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_signin_error/consistency_signin_error_view_controller.h"

#import "google_apis/gaia/google_service_auth_error.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSigninErrorImageName = @"settings_error";

// Layout properties for error message.
constexpr CGFloat kContentSpacing = 16.;
constexpr CGFloat kContentMargin = 16.;

}  // namespace

@interface ConsistencySigninErrorViewController ()

// Authentication error.
@property(nonatomic, assign) GoogleServiceAuthError::State errorState;

// Views for the error message.
@property(nonatomic, strong) UIStackView* contentView;
@property(nonatomic, strong) UIButton* retrySigninButton;

@end

@implementation ConsistencySigninErrorViewController

- (instancetype)initWithAuthErrorState:
    (const GoogleServiceAuthError::State&)errorState {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _errorState = errorState;
  }
  return self;
}

- (void)dealloc {
  [self.retrySigninButton removeTarget:self
                                action:@selector(onRetrySigninButtonPressed:)
                      forControlEvents:UIControlEventTouchDown];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  NSString* title;
  NSString* labelText;
  if (self.errorState ==
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
    title = l10n_util::GetNSString(IDS_IOS_SIGN_IN_AGAIN);
    labelText = l10n_util::GetNSString(IDS_IOS_SIGN_IN_WRONG_CREDENTIALS);
  } else {
    title = l10n_util::GetNSString(IDS_IOS_SIGN_IN_TRY_AGAIN);
    labelText = l10n_util::GetNSString(IDS_IOS_SIGN_IN_AUTH_FAILURE);
    ;
  }

  UIImageView* errorImage = [[UIImageView alloc] init];
  errorImage.translatesAutoresizingMaskIntoConstraints = NO;
  errorImage.image = [UIImage imageNamed:kSigninErrorImageName];

  self.retrySigninButton =
      PrimaryActionButton(/* pointer_interaction_enabled */ YES);
  [self.retrySigninButton setTitle:title forState:UIControlStateNormal];
  [self.retrySigninButton addTarget:self
                             action:@selector(onRetrySigninButtonPressed:)
                   forControlEvents:UIControlEventTouchUpInside];
  self.retrySigninButton.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* messageTitle = [[UILabel alloc] init];
  messageTitle.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  messageTitle.text = l10n_util::GetNSString(IDS_IOS_SIGN_IN_FAILURE_TITLE);
  messageTitle.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* messageSubtitle = [[UILabel alloc] init];
  messageSubtitle.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  messageSubtitle.text = labelText;
  messageSubtitle.translatesAutoresizingMaskIntoConstraints = NO;

  self.contentView = [[UIStackView alloc] initWithArrangedSubviews:@[
    errorImage, messageTitle, messageSubtitle, self.retrySigninButton
  ]];
  self.contentView.axis = UILayoutConstraintAxisVertical;
  self.contentView.alignment = UIStackViewAlignmentCenter;
  self.contentView.spacing = kContentSpacing;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.contentView];

  AddSameCenterConstraints(self.view, self.contentView);
  [NSLayoutConstraint
      activateConstraints:@[ [self.retrySigninButton.widthAnchor
                              constraintEqualToAnchor:self.view.widthAnchor
                                             constant:-kContentMargin] ]];
}

- (void)onRetrySigninButtonPressed:(id)sender {
  [self.delegate consistencySigninErrorViewControllerDidTapRetrySignin:self];
}

#pragma mark - ChildBottomSheetViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  CGFloat contentViewWidth = width - self.view.safeAreaInsets.left -
                             self.view.safeAreaInsets.right -
                             kContentMargin * 2;
  CGSize size = CGSizeMake(contentViewWidth, 0);
  size = [self.contentView
        systemLayoutSizeFittingSize:size
      withHorizontalFittingPriority:UILayoutPriorityRequired
            verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  return size.height +
         self.navigationController.navigationBar.frame.size.height +
         self.navigationController.view.window.safeAreaInsets.bottom +
         kContentMargin * 2;
}

@end
