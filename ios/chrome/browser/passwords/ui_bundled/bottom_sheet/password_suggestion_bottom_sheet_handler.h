// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_HANDLER_H_

@class FormSuggestion;

// Handler for the passwords bottom sheet's context menu.
@protocol PasswordSuggestionBottomSheetHandler

// Displays the password manager menu.
- (void)displayPasswordManager;

// Displays the password details menu.
- (void)displayPasswordDetailsForFormSuggestion:(FormSuggestion*)formSuggestion;

// Handles tapping the primary button. The selected suggestion must be provided.
// `index` represents the position of the suggestion among the available
// suggestions.
- (void)primaryButtonTappedForSuggestion:(FormSuggestion*)formSuggestion
                                 atIndex:(NSInteger)index;

// Handles tapping the secondary button.
- (void)secondaryButtonTapped;

// Handles the view disappearing.
- (void)viewDidDisappear;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_HANDLER_H_
