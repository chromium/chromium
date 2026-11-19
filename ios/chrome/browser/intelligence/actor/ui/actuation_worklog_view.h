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

// Delegate for user interaction events on `ActuationWorklogView`.
@protocol ActuationWorklogViewDelegate <NSObject>

// Called when an accessory card inside the worklog is tapped.
- (void)worklogView:(ActuationWorklogView*)worklogView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem;

@end

// View displaying the list of actuation steps using a timeline.
@interface ActuationWorklogView : UIView

// Delegate receiving interaction events.
@property(nonatomic, weak) id<ActuationWorklogViewDelegate> delegate;

// Whether the worklog is collapsed to its first and in-progress items.
@property(nonatomic, assign, getter=isCollapsed) BOOL collapsed;

// Whether actuation is in progress. While YES, the last item is the current
// step. While NO, no item is current and the tool chip is hidden. Defaults to
// YES.
@property(nonatomic, assign, getter=isActuating) BOOL actuating;

- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

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
