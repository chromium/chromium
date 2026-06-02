// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_STAT_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_STAT_VIEW_H_

#import <UIKit/UIKit.h>

// A card view displaying stats metrics achieved by completed level-up tasks.
@interface LevelUpStatView : UICollectionViewCell

// Sets or updates the stat data displayed in the card.
- (void)setStatTitle:(NSString*)title
            subtitle:(NSString*)subtitle
               image:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_STAT_VIEW_H_
