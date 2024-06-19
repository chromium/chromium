// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_CONSUMER_H_

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_data.h"

@protocol VirtualCardEnrollmentBottomSheetConsumer <NSObject>

// Set the virtual card enrollment bottom sheet data.
- (void)setCardData:(VirtualCardEnrollmentBottomSheetData*)data;

// Shows the loading stating indicating that enrollment is in progress.
- (void)showLoadingState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_CONSUMER_H_
