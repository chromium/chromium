// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_

@class CrURL;

@protocol VirtualCardEnrollmentBottomSheetDelegate <NSObject>

// Called when the user accepted the virtual card enrollment prompt.
- (void)didAccept;

// Called when the user dismissed the virtual card enrollment prompt.
- (void)didCancel;

// Called when the user tapped on a link in the legal message or the learn more
// link.
- (void)didTapLinkURL:(CrURL*)url;

// Called when the view disappeared.
- (void)viewDidDisappear:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_DELEGATE_H_
