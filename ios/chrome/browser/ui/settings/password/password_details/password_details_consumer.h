// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class CredentialDetails;

// Sets the Password details for consumer.
@protocol PasswordDetailsConsumer <NSObject>

// Displays provided array of credential details and the title for the Password
// Details view.
- (void)setCredentials:(NSArray<CredentialDetails*>*)credentials
              andTitle:(NSString*)title;

// Determine if this is a details view for a blocked site (never saved
// password).
- (void)setIsBlockedSite:(BOOL)isBlockedSite;

// Set the signed in user email.
- (void)setUserEmail:(NSString*)userEmail;

// Sets up the share button next to the navigation's right bar button. Tapping
// on the button results in entering the sharing flow when `policyEnabled`.
// Otherwise, info popup is displayed explaining that the feature is disabled by
// policy.
- (void)setupRightShareButton:(BOOL)policyEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_CONSUMER_H_
