// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/main_content_ui_state.h"

#import <ostream>

#import "base/check_op.h"

@interface MainContentUIState ()
// Redefine broadcast properties as readwrite.
@property(nonatomic, assign) CGSize scrollViewSize;
@property(nonatomic, assign) CGSize contentSize;
@property(nonatomic, assign) UIEdgeInsets contentInset;
@property(nonatomic, assign) CGFloat yContentOffset;
@property(nonatomic, assign, getter=isScrolling) BOOL scrolling;
@property(nonatomic, assign, getter=isZooming) BOOL zooming;
@property(nonatomic, assign, getter=isDragging) BOOL dragging;
// Whether the scroll view is decelerating.
@property(nonatomic, assign, getter=isDecelerating) BOOL decelerating;

// Updates `scrolling` based `dragging` and `decelerating`.
- (void)updateIsScrolling;

@end

@implementation MainContentUIState
@synthesize scrollViewSize = _scrollViewSize;
@synthesize contentSize = _contentSize;
@synthesize contentInset = _contentInset;
@synthesize yContentOffset = _yContentOffset;
@synthesize scrolling = _scrolling;
@synthesize zooming = _zooming;
@synthesize dragging = _dragging;
@synthesize decelerating = _decelerating;

- (void)setDragging:(BOOL)dragging {
  if (_dragging == dragging)
    return;
  _dragging = dragging;
  // When a scroll view is being dragged, its contents are tracking the pan
  // gesture, and previous deceleration is cancelled.
  if (_dragging)
    _decelerating = NO;
  [self updateIsScrolling];
}

- (void)setDecelerating:(BOOL)decelerating {
  if (_decelerating == decelerating)
    return;
  _decelerating = decelerating;
  [self updateIsScrolling];
}

#pragma mark Private

- (void)updateIsScrolling {
  self.scrolling = self.dragging || self.decelerating;
}

@end

@interface MainContentUIStateUpdater ()
// The pan gesture driving the current scroll event.
// TODO(crbug.com/41355675): Use this gesture recognizer to broadcast the scroll
// touch location.
@property(nonatomic, weak) UIPanGestureRecognizer* panGesture;
@end

@implementation MainContentUIStateUpdater
@synthesize state = _state;
@synthesize panGesture = _panGesture;

- (instancetype)initWithState:(MainContentUIState*)state {
  if ((self = [super init])) {
    _state = state;
    DCHECK(_state);
  }
  return self;
}

#pragma mark Public

- (void)scrollViewSizeDidChange:(CGSize)scrollViewSize {
  self.state.scrollViewSize = scrollViewSize;
}

- (void)scrollViewDidResetContentSize:(CGSize)contentSize {
  self.state.contentSize = contentSize;
}

- (void)scrollViewDidResetContentInset:(UIEdgeInsets)contentInset {
  self.state.contentInset = contentInset;
}

- (void)scrollViewDidScrollToOffset:(CGPoint)offset {
  self.state.yContentOffset = offset.y;
}

- (void)scrollViewWillBeginDraggingWithGesture:
    (UIPanGestureRecognizer*)panGesture {
  self.state.dragging = YES;
  self.panGesture = panGesture;
}

- (void)scrollViewDidEndDraggingWithGesture:(UIPanGestureRecognizer*)panGesture
                        targetContentOffset:(CGPoint)target {
  // It's possible during the side-swipe gesture for a drag to end on the scroll
  // view without a corresponding begin dragging call.  Early return if there
  // is no pan gesture from the begin call.
  if (!self.panGesture)
    return;
  DCHECK_EQ(panGesture, self.panGesture);
  // UIScrollView does not sent a `-scrollViewDidEndDecelerating:` signal after
  // pixel alignments, so the state should not be considered decelerating if the
  // target content offset is less than a pixel away from the current value.
  CGFloat singlePixel = 1.0 / [UIScreen mainScreen].scale;
  CGFloat delta = std::abs(self.state.yContentOffset - target.y);
  if (delta > singlePixel) {
    self.state.decelerating = YES;
  } else {
    self.state.yContentOffset = target.y;
  }
  self.state.dragging = NO;
  self.panGesture = nil;
}

- (void)scrollViewDidEndDecelerating {
  self.state.decelerating = NO;
}

- (void)scrollViewDidStartZooming {
  self.state.zooming = YES;
}

- (void)scrollViewDidEndZooming {
  self.state.zooming = NO;
}

- (void)scrollWasInterrupted {
  self.state.scrolling = NO;
  self.state.dragging = NO;
  self.state.decelerating = NO;
  self.state.zooming = NO;
}

@end
