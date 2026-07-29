// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

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

// Unified view representing a step in the timeline. When no icon is provided,
// we display a smaller dot view. The subtitle is hidden when nil. Optionally
// links a connector above and below the icon.
//
// +------+  +--------------------+
// | Icon |  | Title              |
// +------+  |--------------------|
//           | Subtitle           |
//           +--------------------+
@interface ActuationWorklogItemView : UIView

// Defines the visibility of the connector lines. Updating this property will
// trigger a layout. Defaults to `ActuationWorklogConnectorVisibility::kNone`.
@property(nonatomic, assign)
    ActuationWorklogConnectorVisibility connectorVisibility;

// Designated initializer
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Configures the view with a worklog item model.
- (void)configureWithItem:(ActuationWorklogItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTUATION_WORKLOG_ITEM_VIEW_H_
