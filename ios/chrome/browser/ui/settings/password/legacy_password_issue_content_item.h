// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_ISSUE_CONTENT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_ISSUE_CONTENT_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"

@protocol PasswordIssue;

// Table view item used by `PasswordIssuesTableViewController`. It is created to
// hold `PasswordIssue`.
@interface LegacyPasswordIssueContentItem : TableViewDetailTextItem

// Associated password issue. Settings this property will change `text` and
// `detailText`.
@property(nonatomic, strong) id<PasswordIssue> password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_LEGACY_PASSWORD_ISSUE_CONTENT_ITEM_H_
