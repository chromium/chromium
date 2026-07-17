// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/details_view_controllers/signin_promo_catalog_view_controller.h"

#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kVerticalMargin = 20;
constexpr CGFloat kHorizontalMargin = 16;
}  // namespace

@implementation SigninPromoCatalogViewController {
  __weak SigninPromoView* _displayedPromoView;
  UISwitch* _styleSwitch;
  UISwitch* _closeButtonSwitch;
  UISwitch* _fakeAccountSwitch;
  UIStackView* _containerStackView;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithNibName:nil bundle:nil];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"Signin Promo";

  [self setupViews];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  if ([self.navigationController respondsToSelector:@selector(closeSettings)]) {
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self.navigationController
                             action:@selector(closeSettings)];
    self.navigationItem.rightBarButtonItem = doneButton;
  }
}

#pragma mark - Private

// Sets up the scroll view, layout stack, controls, and sign-in promo view.
- (void)setupViews {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kVerticalMargin;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.distribution = UIStackViewDistributionFill;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:stackView];
  _containerStackView = stackView;

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [stackView.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor
                       constant:kVerticalMargin],
    [stackView.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor
                       constant:-kVerticalMargin],
    [stackView.leadingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [stackView.trailingAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [stackView.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor
                       constant:-2 * kHorizontalMargin],
  ]];

  // Add Style switch control.
  UIStackView* switchStack = [[UIStackView alloc] init];
  switchStack.axis = UILayoutConstraintAxisHorizontal;
  switchStack.spacing = 10;
  switchStack.alignment = UIStackViewAlignmentCenter;
  switchStack.distribution = UIStackViewDistributionEqualSpacing;

  UILabel* switchLabel = [[UILabel alloc] init];
  switchLabel.text = @"Compact Style";
  switchLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [switchStack addArrangedSubview:switchLabel];

  _styleSwitch = [[UISwitch alloc] init];
  [_styleSwitch addTarget:self
                   action:@selector(styleSwitchChanged:)
         forControlEvents:UIControlEventValueChanged];
  [switchStack addArrangedSubview:_styleSwitch];

  [stackView addArrangedSubview:switchStack];

  // Add Close Button switch control.
  UIStackView* closeButtonStack = [[UIStackView alloc] init];
  closeButtonStack.axis = UILayoutConstraintAxisHorizontal;
  closeButtonStack.spacing = 10;
  closeButtonStack.alignment = UIStackViewAlignmentCenter;
  closeButtonStack.distribution = UIStackViewDistributionEqualSpacing;

  UILabel* closeButtonLabel = [[UILabel alloc] init];
  closeButtonLabel.text = @"Show Close Button";
  closeButtonLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [closeButtonStack addArrangedSubview:closeButtonLabel];

  _closeButtonSwitch = [[UISwitch alloc] init];
  _closeButtonSwitch.on = YES;
  [_closeButtonSwitch addTarget:self
                         action:@selector(closeButtonSwitchChanged:)
               forControlEvents:UIControlEventValueChanged];
  [closeButtonStack addArrangedSubview:_closeButtonSwitch];

  [stackView addArrangedSubview:closeButtonStack];

  // Add Fake Account Switch.
  UIStackView* fakeAccountStack = [[UIStackView alloc] init];
  fakeAccountStack.axis = UILayoutConstraintAxisHorizontal;
  fakeAccountStack.spacing = 10;
  fakeAccountStack.alignment = UIStackViewAlignmentCenter;
  fakeAccountStack.distribution = UIStackViewDistributionEqualSpacing;

  UILabel* fakeAccountLabel = [[UILabel alloc] init];
  fakeAccountLabel.text = @"Fake Account on Device";
  fakeAccountLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [fakeAccountStack addArrangedSubview:fakeAccountLabel];

  _fakeAccountSwitch = [[UISwitch alloc] init];
  [_fakeAccountSwitch addTarget:self
                         action:@selector(fakeAccountSwitchChanged:)
               forControlEvents:UIControlEventValueChanged];
  [fakeAccountStack addArrangedSubview:_fakeAccountSwitch];

  [stackView addArrangedSubview:fakeAccountStack];

  [self updatePromo];
}

// Callback when the style switch is toggled.
- (void)styleSwitchChanged:(UISwitch*)styleSwitch {
  [self updatePromo];
}

// Callback when the close button visibility switch is toggled.
- (void)closeButtonSwitchChanged:(UISwitch*)closeButtonSwitch {
  [self updatePromo];
}

// Callback when the fake account switch is toggled.
- (void)fakeAccountSwitchChanged:(UISwitch*)sender {
  [self updatePromo];
}

// Re-creates the configurator and updates the sign-in promo view.
- (void)updatePromo {
  if (_displayedPromoView) {
    [_displayedPromoView removeFromSuperview];
  }

  SigninPromoView* promoView =
      [[SigninPromoView alloc] initWithFrame:CGRectZero];
  promoView.translatesAutoresizingMaskIntoConstraints = NO;
  promoView.layer.borderWidth = 1.0;
  promoView.layer.borderColor = [UIColor blackColor].CGColor;
  [_containerStackView addArrangedSubview:promoView];
  _displayedPromoView = promoView;

  SigninPromoViewMode mode = _fakeAccountSwitch.on
                                 ? SigninPromoViewModeSigninWithAccount
                                 : SigninPromoViewModeNoAccounts;
  NSString* email = _fakeAccountSwitch.on ? @"fake.user@example.com" : nil;
  NSString* givenName = _fakeAccountSwitch.on ? @"Fake User" : nil;
  UIImage* avatar = nil;
  if (_fakeAccountSwitch.on) {
    CGSize size = GetSizeForIdentityAvatarSize(IdentityAvatarSize::SmallSize);
    avatar = CircularImageFromImage(
        ImageWithColor([UIColor colorNamed:kBlueColor]), size.width);
  }

  SigninPromoViewConfigurator* configurator =
      [[SigninPromoViewConfigurator alloc] initWithSigninPromoViewMode:mode
                                                             userEmail:email
                                                         userGivenName:givenName
                                                             userImage:avatar
                                                        hasCloseButton:NO
                                                      hasSignInSpinner:NO];

  _displayedPromoView.textLabel.text =
      l10n_util::GetNSString(IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS);
  SigninPromoViewStyle style = _styleSwitch.on ? SigninPromoViewStyleCompact
                                               : SigninPromoViewStyleStandard;
  [configurator configureSigninPromoView:_displayedPromoView withStyle:style];
  _displayedPromoView.closeButton.hidden = !_closeButtonSwitch.on;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return YES;
}

@end
