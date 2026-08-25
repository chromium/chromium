// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;
@class LevelUpTaskRowView;

// Delegate protocol to receive row tap notifications.
@protocol LevelUpTaskRowViewDelegate <NSObject>
- (void)taskRowView:(LevelUpTaskRowView*)rowView didTapTask:(LevelUpTask*)task;
@end

// A tap-interactive custom control representing a single task row.
@interface LevelUpTaskRowView : UIControl

// The delegate for this row view.
@property(nonatomic, weak) id<LevelUpTaskRowViewDelegate> delegate;

// Configures the row view with the given task model and separator state.
- (void)configureWithTask:(LevelUpTask*)task showSeparator:(BOOL)showSeparator;

// Configures the row with generic styling and content parameters.
- (void)configureWithTitle:(NSString*)title
               description:(NSString*)description
           backgroundColor:(UIColor*)backgroundColor
           chevronExpanded:(BOOL)chevronExpanded
           separatorHidden:(BOOL)separatorHidden;

// Updates the expansion state of the row's chevron.
- (void)setChevronExpanded:(BOOL)expanded animated:(BOOL)animated;

// Sets whether the row's separator is hidden.
- (void)setSeparatorHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_ROW_VIEW_H_
