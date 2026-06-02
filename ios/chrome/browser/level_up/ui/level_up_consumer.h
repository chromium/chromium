// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"

@class LevelUpTask;
@protocol LevelUpViewControllerDelegate;

// Consumer for the Level Up bottom sheet.
@protocol LevelUpConsumer <NSObject>

@optional
// Sets the active level and list of tasks.
// - level: The user's current Chrome level number.
// - tasks: The array of LevelUpTask objects required for this level.
- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks;

// Adds a new category card to the expanded view.
- (void)addCategoryCard:(LevelUpCategory*)category;

// The delegate to notify the coordinator about card actions.
@property(nonatomic, weak) id<LevelUpViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_CONSUMER_H_
