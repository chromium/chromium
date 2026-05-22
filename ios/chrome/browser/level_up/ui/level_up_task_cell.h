// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_CELL_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_CELL_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;

// Cell representing a single Level Up task row.
@interface LevelUpTaskCell : UITableViewCell

// Configures the cell with the given task model.
- (void)configureWithTask:(LevelUpTask*)task;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_CELL_H_
