// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer to allow the tab group model to send information to the tab group
// UI.
@protocol TabGroupConsumer

// Sets the group title.
- (void)setGroupTitle:(NSString*)title;
// Sets the group color.
- (void)setGroupColor:(UIColor*)color;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_CONSUMER_H_
