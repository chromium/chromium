// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ICON_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ICON_VIEW_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;

// View displaying the Level Up task icon with all necessary styling from task
// parameters.
@interface LevelUpTaskIconView : UIView

// Configures the view for a given LevelUpTask.
- (void)configureWithTask:(LevelUpTask*)task;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ICON_VIEW_H_
