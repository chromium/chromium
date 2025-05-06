// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_mutator.h"
#import "ios/chrome/browser/intelligence/glic/ui/glic_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface GLICConsentViewController () <PromoStyleViewControllerDelegate>
@end

@implementation GLICConsentViewController {
  UIStackView* _mainStackView;
}

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
  self.delegate = self;
  [self configureSheetPresentation];
  [self configurePromoStyleProperties];
  [super viewDidLoad];
  // The stackview should be added after `viewDidLoad`, so that the
  // `UIScrollView` is not placed on top of the stackview.
  [self setupStackView];
}

#pragma mark - Private

// Configure all the stacks.
- (void)setupStackView {
  [self configureMainStackView];
}

// Configure the differents detents for the expanded and collapsed view.
- (void)configureSheetPresentation {
  self.modalPresentationStyle = UIModalPresentationPageSheet;

  self.sheetPresentationController.detents = @[
    [self customHeightDetentWithIdentifier:kGLICConsentPartialDetentIdentifier
                                    height:kGLICConsentPartialDetentHeight],
    [self customHeightDetentWithIdentifier:kGLICConsentFullDetentIdentifier
                                    height:kGLICConsentFullDetentHeight]
  ];

  self.sheetPresentationController.selectedDetentIdentifier =
      kGLICConsentPartialDetentIdentifier;

  self.sheetPresentationController.preferredCornerRadius =
      kGLICConsentPreferredCornerRadius;
  self.sheetPresentationController.prefersScrollingExpandsWhenScrolledToEdge =
      NO;
}

// Create a custom sheet presentation controller detent with a fixed height.
- (UISheetPresentationControllerDetent*)
    customHeightDetentWithIdentifier:(NSString*)identifier
                              height:(CGFloat)height {
  auto resolver = ^CGFloat(
      id<UISheetPresentationControllerDetentResolutionContext> context) {
    return height;
  };

  return
      [UISheetPresentationControllerDetent customDetentWithIdentifier:identifier
                                                             resolver:resolver];
}

// Configure promo style properties to add buttons. Ignores header image type.
- (void)configurePromoStyleProperties {
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  self.primaryActionString = kGLICConsentPrimaryAction;
  self.secondaryActionString = kGLICConsentSecondaryAction;
}

// Configure the main stack view.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.distribution = UIStackViewDistributionFill;
  _mainStackView.alignment = UIStackViewAlignmentFill;
  _mainStackView.spacing = kGLICConsentMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_mainStackView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kGLICConsentMainStackHorizontalInset],
    [_mainStackView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kGLICConsentMainStackHorizontalInset],
    [_mainStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kGLICConsentMainStackTopInset],
    [_mainStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor]
  ]];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.mutator didConsentGLIC];
}

- (void)didTapSecondaryActionButton {
  [self.mutator didRefuseGLICConsent];
}

@end
