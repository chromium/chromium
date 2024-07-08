// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_

@class CrURL;

// Delegate for presentation events from the virtual card enrollment bottom
// sheet view controller.
@protocol VirtualCardEnrollmentBottomSheetDelegate <NSObject>

// Handles the user tapping on a link in the legal message or the learn more
// link. The text of the link is included.
- (void)didTapLinkURL:(CrURL*)URL text:(NSString*)text;

// Handles the view disappearing.
- (void)viewDidDisappear:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_
