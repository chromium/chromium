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

// Sets the `domain` requesting the credentials.
- (void)setDomain:(NSString*)domain;

// Sends the list of suggestions to be presented to the user on the bottom
// sheet.
- (void)setSuggestions:(NSArray<FormSuggestion*>*)suggestions;

// Sets the favicon `faviconAttributes` for the domain displayed in the bottom
// sheet.
- (void)setFaviconAttributes:(FaviconAttributes*)faviconAttributes;

// Sets whether the selected credential is being shared, during which the
// primary button is disabled and shows a spinner.
- (void)setLoading:(BOOL)loading;

// Shows that the selected credential was shared successfully: the primary
// button is disabled and shows a checkmark.
- (void)showConfirmationState;

// Shows that there are no saved passwords for the domain, leaving a single
// button to go back to the remote actor.
- (void)showNoSavedPasswords;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_REMOTE_ACTOR_CREDENTIAL_BOTTOM_SHEET_CONSUMER_H_
