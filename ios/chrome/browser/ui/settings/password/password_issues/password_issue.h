// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_H_

#import <Foundation/Foundation.h>

#import <optional>

namespace password_manager {
struct CredentialUIEntry;
}

@class CrURL;

// Interface used by `PasswordIssueTableViewController` to display items.
@interface PasswordIssue : NSObject

// Associated URL to retrieve a favicon.
@property(nonatomic, readwrite, strong) CrURL* URL;
// Associated website.
@property(nonatomic, copy, readonly) NSString* website;
// Associated username.
@property(nonatomic, copy, readonly) NSString* username;
// Description of type of compromised credential issue.
// Nil for non-compromised credentials.
@property(nonatomic, readonly) NSString* compromisedDescription;
// Credential being displayed in Password Details screen.
@property(nonatomic, readonly) password_manager::CredentialUIEntry credential;
// URL which allows to change the password of compromised credential.
// Can be null for Android credentials not affiliated to a web realm.
@property(nonatomic, readonly) std::optional<CrURL*> changePasswordURL;

// Initializes a PasswordIssue from a CredentialUIEntry.
// Pass `enableCompromisedDescription` as YES when the description of
// compromised issues should be displayed in the UX (e.g., When displaying
// compromised credentials in the Password Issues UX.).
- (instancetype)initWithCredential:
                    (password_manager::CredentialUIEntry)credential
      enableCompromisedDescription:(BOOL)enableCompromisedDescription
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_H_
