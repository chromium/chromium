// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_EDIT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol SaveCardBottomSheetDataSource;
@protocol SaveCardBottomSheetMutator;
@protocol SaveCardBottomSheetDelegate;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ScanCardOfferToSaveAction)
enum class ScanCardOfferToSaveAction {
  kIgnore = 0,
  kAccept = 1,
  kReject = 2,
  kMaxValue = kReject,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:ScanCardOfferToSaveAction)

// View controller for the "Scan and Save" bottom sheet flow.
// It allows users to scan a card and then edit/confirm the details before
// saving.
@interface PaymentsScanSaveAndFillEditViewController
    : ChromeTableViewController <SaveCardBottomSheetConsumer,
                                 CreditCardScannerConsumer>

// Mutator for handling user actions (e.g., saving the edited card).
@property(nonatomic, weak) id<SaveCardBottomSheetMutator> mutator;

// Delegate for handling presentation events (e.g., dismissal).
@property(nonatomic, weak) id<SaveCardBottomSheetDelegate> delegate;

// Data source for the bottom sheet. Provides the logos and accessibility
// labels used in the view.
@property(nonatomic, weak) id<SaveCardBottomSheetDataSource> dataSource;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_SCAN_SAVE_AND_FILL_UI_PAYMENTS_SCAN_SAVE_AND_FILL_EDIT_VIEW_CONTROLLER_H_
