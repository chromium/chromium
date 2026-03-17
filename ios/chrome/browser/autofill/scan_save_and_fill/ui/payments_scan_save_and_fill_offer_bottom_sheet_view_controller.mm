// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_view_controller.h"

#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/scan_save_and_fill/ui/payments_scan_save_and_fill_offer_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Spacing used for the spacing before the logo title in the bottom sheet.
CGFloat const kSpacingBeforeImage = 16;

// Spacing used for the spacing after the logo title in the bottom sheet.
CGFloat const kSpacingAfterImage = 4;

// Height of the logo used as the title of the bottom sheet.
CGFloat const kTitleLogoHeight = 32;

}  // namespace

@interface PaymentsScanSaveAndFillOfferBottomSheetViewController () <
    ConfirmationAlertActionHandler>

@end

@implementation PaymentsScanSaveAndFillOfferBottomSheetViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self titleImage];
  self.customSpacingBeforeImage = kSpacingBeforeImage;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.titleString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_SAVE_AND_FILL_BOTTOM_SHEET_TITLE);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_SAVE_AND_FILL_BOTTOM_SHEET_DESCRIPTION);
  self.subtitleTextStyle = UIFontTextStyleFootnote;
  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_CARD_AND_AUTOFILL_PROMPT_PRIMARY_BUTTON_LABEL);

  self.configuration.secondaryActionString = l10n_util::GetNSString(
      IDS_AUTOFILL_SCAN_CARD_AND_AUTOFILL_PROMPT_CANCEL_BUTTON_LABEL);

  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  [super viewDidLoad];

  [self registerForTraitChanges:TraitCollectionSetForTraits(
                                    @[ UITraitUserInterfaceStyle.class ])
                     withAction:@selector(resizeLogoOnTraitChange)];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
}

- (void)confirmationAlertSecondaryAction {
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  return [super createTableView];
}

- (NSUInteger)rowCount {
  return 0;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  return 0;
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
      CustomSymbolWithPointSize(kGooglePaySymbol, kTitleLogoHeight));
#else
  image = DefaultSymbolTemplateWithPointSize(kDefaultBrowserSymbol,
                                             kTitleLogoHeight);
#endif  // BUILDFLAG(IOS_USE_BRANDED_ASSETS)

  return image;
}

// Resizes the GPay logo to match the new trait collection.
- (void)resizeLogoOnTraitChange {
  self.image = [self titleImage];
}

@end
