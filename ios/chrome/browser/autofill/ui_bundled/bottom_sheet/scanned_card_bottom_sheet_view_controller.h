// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SCANNED_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SCANNED_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol SaveCardBottomSheetMutator;
@protocol SaveCardBottomSheetDelegate;

// View controller for the "Scan and Save" bottom sheet flow.
// It allows users to scan a card and then edit/confirm the details before
// saving.
@interface ScannedCardBottomSheetViewController
    : TableViewBottomSheetViewController <SaveCardBottomSheetConsumer,
                                          CreditCardScannerConsumer>

// Mutator for handling user actions (e.g., saving the edited card).
@property(nonatomic, weak) id<SaveCardBottomSheetMutator> mutator;

// Delegate for handling presentation events (e.g., dismissal).
@property(nonatomic, weak) id<SaveCardBottomSheetDelegate> delegate;

// Data source for the bottom sheet. Provides the logos and accessibility
// labels used in the view.
@property(nonatomic, weak) id<SaveCardBottomSheetDataSource> dataSource;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SCANNED_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_
