// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_

#import <Foundation/Foundation.h>

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
// Credential being displayed in Password Details screen.
@property(nonatomic, readonly) password_manager::CredentialUIEntry credential;

- (instancetype)initWithCredential:
    (password_manager::CredentialUIEntry)credential NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_
