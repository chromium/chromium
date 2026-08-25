// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

@class ActuationWorklogAccessoryItem;
@class ActuationWorklogItem;

// Defines the visibility of the connector lines for the item view.
enum class ActuationWorklogConnectorVisibility {
  // No connector lines are drawn.
  kNone,
  // Only the top connector line is drawn.
  kTop,
  // Only the bottom connector line is drawn.
  kBottom,
  // Both top and bottom connector lines are drawn.
  kBoth,
};

@class ActuationWorklogItemView;

// Delegate protocol for user interaction events on ActuationWorklogItemView.
@protocol ActuationWorklogItemViewDelegate <NSObject>
// Notifies delegate when the item view is tapped.
- (void)worklogItemViewDidTapItem:(ActuationWorklogItemView*)itemView;

// Notifies delegate when an accessory card item inside the item view is tapped.
- (void)worklogItemView:(ActuationWorklogItemView*)itemView
    didTapAccessoryItem:(ActuationWorklogAccessoryItem*)accessoryItem;
@end

// Unified view representing a step in the timeline. When no icon is provided,
// we display a smaller dot view. The subtitle is hidden when nil. Optionally
// links a connector above and below the icon.
//
// +------+  +--------------------+
// | Icon |  | Title              |
// +------+  |--------------------|
//           | Subtitle           |
//           +--------------------+
// +------------------------------+
// |     Overlay buffer area      |
// +------------------------------+
@interface ActuationWorklogItemView : UIView

// Delegate for user interaction events.
@property(nonatomic, weak) id<ActuationWorklogItemViewDelegate> delegate;

// Defines the visibility of the connector lines. Updating this property will
// trigger a layout. Defaults to `ActuationWorklogConnectorVisibility::kNone`.
@property(nonatomic, assign)
    ActuationWorklogConnectorVisibility connectorVisibility;

// Extra vertical space reserved at the bottom of the cell to accommodate
// overlays (such as floating tool chips). Setting a non-zero height extends the
// vertical dashed connector line on the left through the buffer area to ensure
// visual continuity. Defaults to 0.0.
@property(nonatomic, assign) CGFloat bottomBufferHeight;

// Controls whether the item is collapsible and displays a caret indicator.
@property(nonatomic, assign, getter=isCollapsible) BOOL collapsible;

// Reflects whether the item is collapsed. Updating rotates the caret icon.
@property(nonatomic, assign) BOOL collapsed;

// Read-only access to the underlying item model.
@property(nonatomic, readonly) ActuationWorklogItem* item;

// Designated initializer
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Configures the view with a worklog item model.
- (void)configureWithItem:(ActuationWorklogItem*)item;

// Sets the collapsed state with optional rotation animation for the caret.
- (void)setCollapsed:(BOOL)collapsed animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_
