// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_COORDINATOR_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This coordinator is responsible for creating the save card bottom
// sheet's mediator and view controller.
@interface SaveCardBottomSheetCoordinator
    : ChromeCoordinator <SaveCardBottomSheetDelegate>
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_COORDINATOR_H_
