// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_STATE_H_
#define IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_STATE_H_

#import <UIKit/UIKit.h>

// An object encapsulating the broadcasted state of the main scrollable content.
@interface MainContentUIState : NSObject

// The size of the scroll view displaying the main content.
// This should be broadcast using `-broadcastScrollViewSize:`.
@property(nonatomic, readonly) CGSize scrollViewSize;
// The height of the current page's rendered content.
// This should be broadcast using `-broadcastScrollViewContentSize:`.
@property(nonatomic, readonly) CGSize contentSize;
// The content inset of the scroll view displaying the main content.
// This should be broadcast using `-broadcastScrollViewContentInset:`.
@property(nonatomic, readonly) UIEdgeInsets contentInset;
// The vertical offset of the main content.
// This should be broadcast using `-broadcastContentScrollOffset:`.
@property(nonatomic, readonly) CGFloat yContentOffset;
// Whether the scroll view is currently scrolling.
// This should be broadcast using `-broadcastScrollViewIsScrolling:`.
@property(nonatomic, readonly, getter=isScrolling) BOOL scrolling;
// Whether the scroll view is currently zooming.
// This should be broadcast using `-broadcastScrollViewIsZooming:`.
@property(nonatomic, readonly, getter=isZooming) BOOL zooming;
// Whether the scroll view is currently being dragged.
// This should be broadcast using `-broadcastScrollViewIsDragging:`.
@property(nonatomic, readonly, getter=isDragging) BOOL dragging;

@end

// Helper object that uses scroll view events to update a MainContentUIState.
@interface MainContentUIStateUpdater : NSObject

// The state being updated by this object.
@property(nonatomic, readonly, strong, nonnull) MainContentUIState* state;

// Designated initializer for an object that updates `state`.
- (nullable instancetype)initWithState:(nonnull MainContentUIState*)state
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

// Called to broadcast changes in the scroll view's size.
- (void)scrollViewSizeDidChange:(CGSize)scrollViewSize;
// Called to broadcast changes in the content size.
- (void)scrollViewDidResetContentSize:(CGSize)contentSize;
// Called to broadcast changes in the content inset.
- (void)scrollViewDidResetContentInset:(UIEdgeInsets)contentInset;
// Called to broadcast scroll offset changes due to scrolling.
- (void)scrollViewDidScrollToOffset:(CGPoint)offset;
// Called when a drag event with `panGesture` begins.
- (void)scrollViewWillBeginDraggingWithGesture:
    (nonnull UIPanGestureRecognizer*)panGesture;
// Called when a drag event with `panGesture` ends.  `target` is the final
// content offset resulting from the residual velocity of the drag event.
- (void)scrollViewDidEndDraggingWithGesture:
            (nonnull UIPanGestureRecognizer*)panGesture
                        targetContentOffset:(CGPoint)target;
// Called when the scroll view stops decelerating.
- (void)scrollViewDidEndDecelerating;
// Called when the scroll view starts and ends zooming.
- (void)scrollViewDidStartZooming;
- (void)scrollViewDidEndZooming;
// Called when a scroll event is interrupted (i.e. when a navigation occurs mid-
// scroll).
- (void)scrollWasInterrupted;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_CONTENT_MAIN_CONTENT_UI_STATE_H_
