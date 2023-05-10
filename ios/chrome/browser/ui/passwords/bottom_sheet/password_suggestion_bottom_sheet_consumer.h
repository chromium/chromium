// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

@class FormSuggestion;

// Suggestions consumer for the passwords bottom sheet.
@protocol PasswordSuggestionBottomSheetConsumer

// Sends the list of suggestions to be presented to the user on the bottom sheet
// and the current page's domain.
- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions
             andDomain:(NSString*)domain;

// Request to dismiss the bottom sheet.
- (void)dismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
