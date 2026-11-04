// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_CONSUMER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_CONSUMER_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;
@class FormSuggestion;

// Consumer for the Remote Actor Credential Bottom Sheet.
@protocol RemoteActorCredentialBottomSheetConsumer <NSObject>

// Sends the list of suggestions to be presented to the user on the bottom sheet
// for the requested domain.
- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions
             andDomain:(NSString*)domain;

// Sets the favicon `faviconAttributes` for the domain displayed in the bottom
// sheet.
- (void)setFaviconAttributes:(FaviconAttributes*)faviconAttributes;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_CONSUMER_H_
