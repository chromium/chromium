// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_STAT_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_STAT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/model/task_types.h"

// Model for a completed task stat card.
@interface LevelUpStat : NSObject

// Stat title.
@property(nonatomic, copy, readonly) NSString* title;

// Stat subtitle.
@property(nonatomic, copy, readonly) NSString* subtitle;

// Stat icon image.
@property(nonatomic, strong, readonly) UIImage* image;

// Stat type.
@property(nonatomic, assign, readonly) LevelUpTaskStatType type;

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                        image:(UIImage*)image
                         type:(LevelUpTaskStatType)type;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_STAT_H_
