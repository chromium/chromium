// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationWorklogAccessoryItem;
@class ActuationWorklogChip;
@class ActuationWorklogItem;
@class ActuationWorklogView;

// Delegate protocol for ActuationWorklogView interaction events.
@protocol ActuationWorklogViewDelegate <NSObject>
// Notifies the delegate when the worklog collapsed state changes.
- (void)worklogView:(ActuationWorklogView*)worklogView
    didChangeCollapsed:(BOOL)collapsed;

// Notifies delegate when an accessory card item inside the worklog is tapped.
- (void)worklogView:(ActuationWorklogView*)worklogView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem;
@end

// View displaying the list of actuation steps using a timeline.
@interface ActuationWorklogView : UIView

// Delegate for collapse events.
@property(nonatomic, weak) id<ActuationWorklogViewDelegate> delegate;

// Reflects whether the worklog is collapsed.
@property(nonatomic, assign) BOOL collapsed;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Sets the collapsed state with optional animation.
- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated;

// Sets or replaces all timeline items instantly.
- (void)setItems:(NSArray<ActuationWorklogItem*>*)items;

// Appends a single item to the end of the timeline log.
- (void)addItem:(ActuationWorklogItem*)item;

// Mutates the last item at the end of the timeline log. Pass nil to remove the
// last item.
- (void)setLastItem:(ActuationWorklogItem*)item;

// Sets or clears the current actor tool chip at the end of the timeline.
// Passing nil hides the chip.
- (void)setChip:(ActuationWorklogChip*)chip;

// Resets all timeline items, chip, and collapse state.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_
