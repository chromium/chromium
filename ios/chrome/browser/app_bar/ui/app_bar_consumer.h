// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_

// Consumer of the app bar.
@protocol AppBarConsumer <NSObject>

// Updates the tab count displayed in the app bar.
- (void)updateTabCount:(NSUInteger)count;

// Sets whether the tab grid is visible or not.
- (void)setTabGridVisible:(BOOL)tabGridVisible;

// Sets whether the tab groups page in the tab grid is visible.
- (void)setTabGroupsPageVisible:(BOOL)tabGroupsPageVisible;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONSUMER_H_
