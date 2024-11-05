// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_ACTIVITY_SUMMARY_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_ACTIVITY_SUMMARY_CELL_H_

#import <UIKit/UIKit.h>

// Cell representing the activity summary in a shared group.
@interface TabGroupActivitySummaryCell : UICollectionViewCell

// The text of summary.
@property(nonatomic, strong) NSString* text;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUP_ACTIVITY_SUMMARY_CELL_H_
