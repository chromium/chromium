// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"

#import "base/check.h"

ChromeBroadcastObserverInterface::~ChromeBroadcastObserverInterface() = default;

@implementation ChromeBroadcastOberverBridge
@synthesize observer = _observer;

- (instancetype)initWithObserver:(ChromeBroadcastObserverInterface*)observer {
  if (self = [super init]) {
    _observer = observer;
    DCHECK(_observer);
  }
  return self;
}

- (void)broadcastScrollViewSize:(CGSize)scrollViewSize {
  self.observer->OnScrollViewSizeBroadcasted(scrollViewSize);
}

- (void)broadcastScrollViewContentSize:(CGSize)contentSize {
  self.observer->OnScrollViewContentSizeBroadcasted(contentSize);
}

- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset {
  self.observer->OnScrollViewContentInsetBroadcasted(contentInset);
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  self.observer->OnContentScrollOffsetBroadcasted(offset);
}

- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling {
  self.observer->OnScrollViewIsScrollingBroadcasted(scrolling);
}

- (void)broadcastScrollViewIsZooming:(BOOL)zooming {
  self.observer->OnScrollViewIsZoomingBroadcasted(zooming);
}

- (void)broadcastScrollViewIsDragging:(BOOL)dragging {
  self.observer->OnScrollViewIsDraggingBroadcasted(dragging);
}

- (void)broadcastCollapsedTopToolbarHeight:(CGFloat)height {
  self.observer->OnCollapsedTopToolbarHeightBroadcasted(height);
}

- (void)broadcastExpandedTopToolbarHeight:(CGFloat)height {
  self.observer->OnExpandedTopToolbarHeightBroadcasted(height);
}

- (void)broadcastExpandedBottomToolbarHeight:(CGFloat)height {
  self.observer->OnExpandedBottomToolbarHeightBroadcasted(height);
}

- (void)broadcastCollapsedBottomToolbarHeight:(CGFloat)height {
  self.observer->OnCollapsedBottomToolbarHeightBroadcasted(height);
}

@end
