// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_mutator.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface GLICConsentViewController () <PromoStyleViewControllerDelegate>

@end

@implementation GLICConsentViewController

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
  self.delegate = self;
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  self.modalPresentationStyle = UIModalPresentationPageSheet;

  // TODO(crbug.com/414777890): Use a custom detent.
  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent]
  ];

  self.sheetPresentationController.preferredCornerRadius = 16.0;

  UIView* rootView = [[UIView alloc] initWithFrame:UIScreen.mainScreen.bounds];
  rootView.backgroundColor = [UIColor systemBackgroundColor];
  [self.view addSubview:rootView];

  // TODO(crbug.com/414778685): Add strings.
  self.primaryActionString = @"Yes, I'm in";
  self.secondaryActionString = @"No thanks";

  self.bannerSize = BannerImageSizeType::kStandard;
  [super viewDidLoad];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.mutator didConsentGLIC];
}

- (void)didTapSecondaryActionButton {
  [self.mutator didRefuseGLICConsent];
}

@end
