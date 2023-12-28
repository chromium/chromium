// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_CONSUMER_H_

#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue_group.h"

// Consumer for the Password Issues Screen.
@protocol PasswordIssuesConsumer <NSObject>

// Passes password issues to the consumer.
- (void)setPasswordIssues:(NSArray<PasswordIssueGroup*>*)passwordGroups
    dismissedWarningsCount:(NSInteger)dismissedWarnings;

// Sets the navigation bar title.
- (void)setNavigationBarTitle:(NSString*)title;

// Sets the header on top of the page with an optional link.
- (void)setHeader:(NSString*)text URL:(CrURL*)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_PASSWORD_ISSUES_CONSUMER_H_
