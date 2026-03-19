// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/age_mismatch_signout_view_controller.h"

#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

// TODO(crbug.com/483935544): Integrate IdentityButtonControl.
@implementation AgeMismatchSignoutViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kImage;
  self.headerBackgroundImage =
      [UIImage imageNamed:@"age_mismatch_prompt_image"];

  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  self.titleText =
      l10n_util::GetNSString(isIPad ? IDS_IOS_AGE_MISMATCH_HEADER_IPAD
                                    : IDS_IOS_AGE_MISMATCH_HEADER_IPHONE);

  self.subtitleText =
      l10n_util::GetNSString(isIPad ? IDS_IOS_AGE_MISMATCH_SUBTITLE_IPAD
                                    : IDS_IOS_AGE_MISMATCH_SUBTITLE_IPHONE);

  self.disclaimerText = l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_DISCLAIMER);

  // TODO(crbug.com/483935544): Update the disclaimer URL.
  self.disclaimerURLs = @[ [[NSURL alloc] initWithString:@""] ];

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_PRIMARY_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_AGE_MISMATCH_SECONDARY_BUTTON);
  [super viewDidLoad];
}

@end
