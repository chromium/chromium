// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_PRESENTER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_PRESENTER_H_

#import <Foundation/Foundation.h>

// Interface to control the presentation of the password suggestions bottom
// sheet.
@protocol PasswordSuggestionBottomSheetPresenter <NSObject>

// Ends the presentation of the bottom sheet.
- (void)endPresentation;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_PRESENTER_H_
