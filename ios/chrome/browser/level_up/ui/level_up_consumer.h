// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for the Level Up bottom sheet.
@protocol LevelUpConsumer

// Sets the active level progress metrics.
// - level: The user's current Chrome level number.
// - completedTasksForLevel: The number of user-completed tasks within the
// current active level.
// - totalTasksForLevel: The total number of tasks required to complete the
// current active level.
- (void)setLevel:(NSInteger)level
    completedTasksForLevel:(NSInteger)completedTasksForLevel
        totalTasksForLevel:(NSInteger)totalTasksForLevel;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_
