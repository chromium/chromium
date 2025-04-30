// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"

@implementation GlicConsentViewController

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
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

@end
