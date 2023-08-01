// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_COORDINATOR_DELEGATE_H_

@class PasswordSuggestionBottomSheetCoordinator;

@protocol PasswordSuggestionBottomSheetCoordinatorDelegate <NSObject>

- (void)passwordSuggestionBottomSheetCoordinatorWantsToBeStopped:
    (PasswordSuggestionBottomSheetCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_COORDINATOR_DELEGATE_H_
