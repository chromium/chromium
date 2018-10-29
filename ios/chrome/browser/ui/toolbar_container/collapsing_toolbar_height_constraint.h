// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar_container/toolbar_height_range.h"

@protocol CollapsingToolbarHeightConstraintDelegate;

// A constraint that scales between a collapsed and expanded height value.
@interface CollapsingToolbarHeightConstraint : NSLayoutConstraint

// Returns a constraint that manages the height of |view|.  If |view| conforms
// to ToolbarCollapsing, updating |progress| will scale between its collapsed
// and expanded heights.  Otherwise, the constraint will lock |view|'s height
// to its intrinisic content height.  The height range can be increased using
// |additionalHeight|.
+ (nullable instancetype)constraintWithView:(nonnull UIView*)view;

// Used to add additional height to the toolbar.
@property(nonatomic, assign) CGFloat additionalHeight;
// Whether the additional height should be collapsed.
@property(nonatomic, assign) BOOL collapsesAdditionalHeight;

// The height range for the constraint.  If the constrained view conforms to
// ToolbarCollapsing, the range will be populated using the collapsed and
// expanded toolbar heights from that protocol, otherwise the intrinsic content
// height is used.  |additionalHeight| and is added to the max height, and
// optionally added to the min height if |collapsesAdditionalHeight| is NO.
@property(nonatomic, readonly)
    const toolbar_container::HeightRange& heightRange;

// The interpolation progress within the height range to use for the
// constraint's constant.  The value is clamped between 0.0 and 1.0.
@property(nonatomic, assign) CGFloat progress;

// The constraint's delegate.
@property(nonatomic, weak, nullable)
    id<CollapsingToolbarHeightConstraintDelegate>
        delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_
