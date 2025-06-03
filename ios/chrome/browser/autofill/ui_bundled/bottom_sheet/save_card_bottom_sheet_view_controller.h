// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mutator.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller.h"

@protocol SaveCardBottomSheetDataSource;

// The view controller to configure save card bottomsheet UI.
@interface SaveCardBottomSheetViewController
    : TableViewBottomSheetViewController <SaveCardBottomSheetConsumer>

// Mutator to handle user actions.
@property(nonatomic, weak) id<SaveCardBottomSheetMutator> mutator;

// Data source to get on demand data.
@property(nonatomic, weak) id<SaveCardBottomSheetDataSource> dataSource;

// Delegate to handle navigational events (e.g tap on legal message link opens
// url in a new tab, bottomsheet gets swiped or a new tab is opened).
@property(nonatomic, weak) id<SaveCardBottomSheetDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_VIEW_CONTROLLER_H_
