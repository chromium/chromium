// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationWorklogAccessoryItem;
@class ActuationWorklogChip;
@class ActuationWorklogItem;
@class ActuationWorklogItemView;
@protocol ActuationWorklogItemViewDelegate;

// View displaying the list of actuation steps using a timeline.
@interface ActuationWorklogView : UIView

// Delegate for receiving interaction events from item views.
@property(nonatomic, weak) id<ActuationWorklogItemViewDelegate> itemDelegate;

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

// Handles a tap event on `itemView`, toggling collapse if it is the header
// item.
- (void)handleTapOnView:(ActuationWorklogItemView*)itemView;

// Resets all timeline items, chip, and collapse state.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_VIEW_H_
