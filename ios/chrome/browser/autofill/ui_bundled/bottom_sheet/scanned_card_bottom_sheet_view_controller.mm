// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/scanned_card_bottom_sheet_view_controller.h"

@implementation ScannedCardBottomSheetViewController

#pragma mark - SaveCardBottomSheetConsumer

- (void)setCardNameAndLastFourDigits:(NSString*)label
                  withCardExpiryDate:(NSString*)subLabel
                         andCardIcon:(UIImage*)issuerIcon
           andCardAccessibilityLabel:(NSString*)accessibilityLabel {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol,
  // but this UI uses CreditCardScannerConsumer for the card details instead.
}

- (void)showLoadingStateWithAccessibilityLabel:(NSString*)accessibilityLabel {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)showConfirmationState {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setAboveTitleImage:(UIImage*)logoImage {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setAboveTitleImageDescription:(NSString*)description {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setSubtitle:(NSString*)subtitle {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setAcceptActionText:(NSString*)acceptActionText {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setCancelActionText:(NSString*)cancelActionText {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

- (void)setLegalMessages:(NSArray*)legalMessages {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol.
}

#pragma mark - CreditCardScannerConsumer

- (void)setCreditCardNumber:(NSString*)cardNumber
            expirationMonth:(NSString*)expirationMonth
             expirationYear:(NSString*)expirationYear {
  // Empty implementation: Required by SaveCardBottomSheetConsumer protocol,
  // but this UI uses CreditCardScannerConsumer for the card details instead.
}

#pragma mark - TableViewBottomSheetViewController

- (NSUInteger)rowCount {
  // TODO(crbug.com/484102792): Return the actual number of rows once the
  // editable form UI (Card Number, Expiration, Name, CVC) is implemented.
  return 1;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  // TODO(crbug.com/484102792): Calculate and return the actual height for
  // each cell once the editable form UI is implemented.
  return 0.0;
}

@end
