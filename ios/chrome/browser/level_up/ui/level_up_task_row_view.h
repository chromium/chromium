// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;

// A tap-interactive custom control representing a single task row.
@interface LevelUpTaskRowView : UIControl

// Configures the row view with the given task model and separator state.
- (void)configureWithTask:(LevelUpTask*)task showSeparator:(BOOL)showSeparator;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_
