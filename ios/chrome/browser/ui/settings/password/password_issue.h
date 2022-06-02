// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_

#import <Foundation/Foundation.h>

@class CrURL;

// Protocol used by `PasswordIssueTableViewController` to display items.
@protocol PasswordIssue

// Associated URL to retrieve a favicon.
@property(nonatomic, readwrite, strong) CrURL* URL;
// Associated website.
@property(nonatomic, readonly) NSString* website;
// Associated username.
@property(nonatomic, readonly) NSString* username;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUE_H_
