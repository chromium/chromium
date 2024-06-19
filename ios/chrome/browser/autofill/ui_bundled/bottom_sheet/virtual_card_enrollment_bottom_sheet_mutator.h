// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MUTATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MUTATOR_H_

#import <Foundation/Foundation.h>

@protocol VirtualCardEnrollmentBottomSheetMutator <NSObject>

// Called when the user accepted the virtual card enrollment prompt.
- (void)didAccept;

// Called when the user dismissed the virtual card enrollment prompt.
- (void)didCancel;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BOTTOM_SHEET_VIRTUAL_CARD_ENROLLMENT_BOTTOM_SHEET_MUTATOR_H_
