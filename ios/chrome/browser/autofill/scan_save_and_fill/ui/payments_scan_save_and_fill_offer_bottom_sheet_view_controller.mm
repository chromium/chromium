// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_view_controller.h"

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Spacing used for the spacing before the logo title in the bottom sheet.
CGFloat const kSpacingBeforeImage = 32;

// Spacing used for the spacing after the logo title in the bottom sheet.
CGFloat const kSpacingAfterImage = 24;

// Height of the logo used as the title of the bottom sheet.
CGFloat const kTitleLogoHeight = 32;
// Side length for the custom favicon.
CGFloat const kCustomFaviconSideLength = 58;

}  // namespace

@interface PaymentsScanSaveAndFillOfferBottomSheetViewController () <
    ConfirmationAlertActionHandler>

@end

@implementation PaymentsScanSaveAndFillOfferBottomSheetViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self titleImage];
  self.imageBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.imageEnclosedWithShadowWithoutBadge = YES;
  self.customFaviconSideLength = kCustomFaviconSideLength;
  self.imageEnclosedWithShadowAndBadge = NO;
  self.customSpacingBeforeImage = kSpacingBeforeImage;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.imageHasFixedSize = YES;

  self.titleString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_SAVE_AND_FILL_BOTTOM_SHEET_TITLE);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_SAVE_AND_FILL_BOTTOM_SHEET_DESCRIPTION);
  self.subtitleTextStyle = UIFontTextStyleFootnote;
  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_CARD_AND_AUTOFILL_PROMPT_PRIMARY_BUTTON_LABEL);
  self.configuration.primaryActionImage = DefaultSymbolTemplateWithPointSize(
      kCreditCardFinderActionSymbol, kSymbolActionPointSize);
  self.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_CARD_AND_AUTOFILL_PROMPT_CANCEL_BUTTON_LABEL);

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  self.topAlignedLayout = YES;
  self.addsContentViewBottomInset = NO;

  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;
  [spacerView.heightAnchor constraintEqualToConstant:0].active = YES;
  self.underTitleView = spacerView;

  [super viewDidLoad];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  [self.delegate paymentsBottomSheetViewDidAppear];

  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.imageViewAccessibilityLabel);
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];

  [self.delegate paymentsBottomSheetDidDisappear];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate didTapScanCardButton];
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate didTapOnCancelButton];
}

#pragma mark - UIResponder

- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Private

// Returns the title logo image that is resized to the correct size for the
// bottom sheet.
- (UIImage*)titleImage {
  UIImage* image;
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  image = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleWalletIconSymbol, kTitleLogoHeight));
#else
  image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                             kTitleLogoHeight);
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

  return image;
}

@end
