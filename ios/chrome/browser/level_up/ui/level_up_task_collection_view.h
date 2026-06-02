// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;
@class LevelUpTaskCollectionView;

// Delegate protocol for actions inside LevelUpTaskCollectionView card.
@protocol LevelUpTaskCollectionViewDelegate <NSObject>

// Called when the user taps the "See All" button on the tasks checklist card.
- (void)didTapSeeAllTasks:(UICollectionViewCell*)cell;

@end

// Collection view cell representing the Level Up checklist tasks card.
@interface LevelUpTaskCollectionView : UICollectionViewCell

// The card header title.
@property(nonatomic, copy) NSString* headerTitle;

// Whether to display the "See All" action button.
@property(nonatomic, assign) BOOL showsSeeAllButton;

// Delegate receiving user action callbacks.
@property(nonatomic, weak) id<LevelUpTaskCollectionViewDelegate> delegate;

// Populates the checklist card cell with the given level and tasks.
- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_H_
