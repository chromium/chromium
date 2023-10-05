// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_URL_DRAG_DROP_HANDLER_H_
#define IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_URL_DRAG_DROP_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

class GURL;
@class URLInfo;

// The interface for providing a draggable URL from a view.
@protocol URLDragDataSource
// Returns a wrapper object with URL and title for dragging from `view`. Returns
// nil if `view` is not currently draggable.
- (URLInfo*)URLInfoForView:(UIView*)view;
// Returns the visible path for the `view` used for the drag preview.
- (UIBezierPath*)visiblePathForView:(UIView*)view;
@end

// The interface for handling URL drops in a view.
@protocol URLDropDelegate
// Returns whether `view` is in a state to handle URL drops.
- (BOOL)canHandleURLDropInView:(UIView*)view;
// Provides the receiver with the dropped `URL`, which was dropped at `point` in
// the coordinate space of the `view`'s bounds.
- (void)view:(UIView*)view didDropURL:(const GURL&)URL atPoint:(CGPoint)point;
@end

// A delegate object that is configured to handle single URL drags and drops
// from a view.
@interface URLDragDropHandler
    : NSObject <UIDragInteractionDelegate, UIDropInteractionDelegate>
// Origin used to configure drag items.
@property(nonatomic, assign) WindowActivityOrigin origin;
// The data source object that provides draggable URLs from a view.
@property(nonatomic, weak) id<URLDragDataSource> dragDataSource;
// The delegate object that manages URL drops onto a view.
@property(nonatomic, weak) id<URLDropDelegate> dropDelegate;
@end

#endif  // IOS_CHROME_BROWSER_DRAG_AND_DROP_MODEL_URL_DRAG_DROP_HANDLER_H_
