// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_COMPACT_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_COMPACT_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationWorklogChip;
@class ActuationWorklogCompactView;
@class ActuationWorklogItem;

// Delegate for handling height change updates during transitions.
@protocol ActuationWorklogCompactViewDelegate <NSObject>
// Called when the view's height changes due to a layout transition.
- (void)worklogCompactView:(ActuationWorklogCompactView*)view
           didChangeHeight:(CGFloat)targetHeight;
@end

// Compact view displaying a single active step in the timeline.
// Smoothly translates between steps vertically. A safety mechanism is added
// to queue transitions in case they happen too quickly.
@interface ActuationWorklogCompactView : UIView

// The delegate for height update notifications.
@property(nonatomic, weak) id<ActuationWorklogCompactViewDelegate> delegate;

// Designated initializer.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Transition to display a new item and its associated tool chip.
- (void)transitionToItem:(ActuationWorklogItem*)item
                    chip:(ActuationWorklogChip*)chip
                animated:(BOOL)animated;

// Resets and purges all current step views and queued transitions.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_COMPACT_VIEW_H_
