// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class FormSuggestion;

// Suggestions consumer for the credential bottom sheet.
@protocol CredentialSuggestionBottomSheetConsumer <NSObject>

// Sends the list of suggestions to be presented to the user on the bottom sheet
// and the current page's domain.
- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions
             andDomain:(NSString*)domain;

// Sends title and subtitle to be presented to the user on the bottom sheet.
// Might not be called for every consumer, in which case they might set their
// own defaults or not display those at all.
- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle;

// Sends the image to be presented to the user on the bottom sheet. Might not be
// called for every consumer, in which case they might set their own defaults or
// not display it all.
- (void)setAvatarImage:(UIImage*)avatarImage;

// Sets the primary action label.
- (void)setPrimaryActionString:(NSString*)primaryActionString;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_UI_BUNDLED_BOTTOM_SHEET_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_CONSUMER_H_
