// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROGRESS_BAR_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROGRESS_BAR_H_

#import <UIKit/UIKit.h>

// A reusable multi-segment progress bar for Level Up.
@interface LevelUpProgressBar : UIView

// Sets the number of completed and total tasks.
- (void)setCompleted:(NSInteger)completed total:(NSInteger)total;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_PROGRESS_BAR_H_
