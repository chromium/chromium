// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_DELEGATE_H_

#import <Foundation/Foundation.h>

@class CrURL;

// Delegate to handle navigational events from the save card bottomsheet view
// controller.
@protocol SaveCardBottomSheetDelegate <NSObject>

// Handles user tapping on a link in the legal message.
- (void)didTapLinkURL:(CrURL*)URL;

// Handles bottomsheet's dismissal. Bottomsheet could be dismissed if user
// swipes away the bottomsheet, in that case the coordinator's owner may not
// stop the coordinator immediately. This ensures coordinator is stopped and
// resources owned by it are freed as soon as possible.
- (void)onViewDisappeared;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_SAVE_CARD_BOTTOM_SHEET_DELEGATE_H_
