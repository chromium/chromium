// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_

#import <UIKit/UIKit.h>

// Protocol collecting all of the methods that broadcast keys will trigger
// in an observer. Each key maps to a specific observer method as indicated.
// (this mapping is generated in the implementation of the Broadcaster class).
//
// All of the methods in this protocol *must* return void and take exactly one
// argument.
@protocol ChromeBroadcastObserver<NSObject>
@optional

#pragma mark - Scrolling events

// Observer method for objects that care about the size of the scroll view
// displaying the main content.
- (void)broadcastScrollViewSize:(CGSize)scrollViewSize;

// Observer method for objects that care about the height of the current page's
// rendered contents.
- (void)broadcastScrollViewContentSize:(CGSize)contentSize;

// Observer method for objects that care about the content inset for the scroll
// view displaying the main content area.
- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset;

// Observer method for objects that care about the current vertical (y-axis)
// scroll offset of the tab content area.
- (void)broadcastContentScrollOffset:(CGFloat)offset;

// Observer method for objects that care about whether the main content area is
// scrolling.
- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling;

// Observer method for objects that care about whether the main content area is
// zooming.
- (void)broadcastScrollViewIsZooming:(BOOL)zooming;

// Observer method for objects that care about whether the main content area is
// being dragged.  Note that if a drag ends with residual velocity, it's
// possible for `dragging` to be NO while `scrolling` is still YES.
- (void)broadcastScrollViewIsDragging:(BOOL)dragging;

#pragma mark - Toolbar UI

// Observer method for objects that care about the collapsed top toolbar height.
// The value broadcast by this method is the distance by which the toolbar
// overlaps the browser content area after the toolbar has been collapsed due
// to scroll events.
- (void)broadcastCollapsedTopToolbarHeight:(CGFloat)height;

// Observer method for objects that care about the fully expanded top toolbar
// height.  The value broadcast by this method is the distance by which the
// toolbar overlaps the browser content area after the toolbar when the toolbar
// is fully visible (i.e. after a page load).  When scrolling occurs, the
// toolbar overlap distance will be reduced to the collapsed height.
- (void)broadcastExpandedTopToolbarHeight:(CGFloat)height;

// Observer method for objects that care about the height of the bottom toolbar.
// The value broadcast by this method is the distance by which the toolbar
// overlaps the browser content area after the toolbar when the toolbar is fully
// visible (i.e. after a page load).  When scrolling occurs, the toolbar overlap
// distance will be reduced to the collapsed height.
- (void)broadcastExpandedBottomToolbarHeight:(CGFloat)height;

// Observer method for objects that care about the collapsed bottom toolbar
// height. The value broadcast by this method is the distance by which the
// toolbar overlaps the browser content area after the toolbar has been
// collapsed due to scroll events.
- (void)broadcastCollapsedBottomToolbarHeight:(CGFloat)height;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
