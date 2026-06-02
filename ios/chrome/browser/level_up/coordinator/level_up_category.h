// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_CATEGORY_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_CATEGORY_H_

#import <Foundation/Foundation.h>

@class LevelUpTask;

// Model for a task category.
@interface LevelUpCategory : NSObject

// Category title.
@property(nonatomic, copy, readonly) NSString* title;

// List of tasks.
@property(nonatomic, copy, readonly) NSArray<LevelUpTask*>* tasks;

- (instancetype)initWithTitle:(NSString*)title
                        tasks:(NSArray<LevelUpTask*>*)tasks;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_CATEGORY_H_
