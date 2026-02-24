// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_CONSUMER_H_

// Consumer for the "Quick Delete Other Data" page.
@protocol QuickDeleteOtherDataConsumer <NSObject>

// Sets the title for the "Quick Delete Other data"
// page.
- (void)setOtherDataPageTitle:(NSString*)title;

// Sets the subtitle for the "Search history" cell.
- (void)setSearchHistoryCellSubtitle:(NSString*)subtitle;

// Sets whether the "My Activity" cell should be shown.
- (void)setShouldShowMyActivityCell:(BOOL)shouldShowMyActivityCell;

// Sets whether the "Search history" cell should be shown.
- (void)setShouldShowSearchHistoryCell:(BOOL)shouldShowSearchHistoryCell;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_UI_QUICK_DELETE_OTHER_DATA_CONSUMER_H_
