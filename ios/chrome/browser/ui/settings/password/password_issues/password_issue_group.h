// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_GROUP_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_GROUP_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"

// Data model for PasswordIssuesTableViewController.
// Represents a set of password issues displayed together in the UI with an
// optional text header on top.
@interface PasswordIssueGroup : NSObject

// Text displayed in a header on top of the password issues in this group.
@property(nonatomic, copy, readonly) NSString* headerText;

// Password issues displayed together in this group.
@property(nonatomic, copy, readonly) NSArray<PasswordIssue*>* passwordIssues;

- (instancetype)initWithHeaderText:(NSString*)headerText
                    passwordIssues:(NSArray<PasswordIssue*>*)passwordIssues
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUE_GROUP_H_
