// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcast_observer_bridge.h"

#import "base/check.h"
#import "ios/web/common/features.h"

ChromeBroadcastObserverInterface::~ChromeBroadcastObserverInterface() = default;

@implementation ChromeBroadcastOberverBridge
@synthesize observer = _observer;

- (instancetype)initWithObserver:(ChromeBroadcastObserverInterface*)observer {
  if ((self = [super init])) {
    _observer = observer;
    DCHECK(_observer);
  }
  return self;
}

- (void)broadcastScrollViewSize:(CGSize)scrollViewSize {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
  self.observer->OnScrollViewSizeBroadcasted(scrollViewSize);
}

- (void)broadcastScrollViewContentSize:(CGSize)contentSize {
  if (contentSize.width == 0.0 && contentSize.height == 0.0) {
    return;
  }
  self.observer->OnScrollViewContentSizeBroadcasted(contentSize);
}

- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
  self.observer->OnScrollViewContentInsetBroadcasted(contentInset);
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
  self.observer->OnContentScrollOffsetBroadcasted(offset);
}

- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
  self.observer->OnScrollViewIsScrollingBroadcasted(scrolling);
}

- (void)broadcastScrollViewIsZooming:(BOOL)zooming {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
  self.observer->OnScrollViewIsZoomingBroadcasted(zooming);
}

- (void)broadcastScrollViewIsDragging:(BOOL)dragging {
  CHECK(web::features::ShouldUseBroadcasterForSmoothScrolling());
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
