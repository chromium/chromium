// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"

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
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  self.observer->OnScrollViewSizeBroadcasted(scrollViewSize);
}

- (void)broadcastScrollViewContentSize:(CGSize)contentSize {
  self.observer->OnScrollViewContentSizeBroadcasted(contentSize);
}

- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  self.observer->OnScrollViewContentInsetBroadcasted(contentInset);
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  self.observer->OnContentScrollOffsetBroadcasted(offset);
}

- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  self.observer->OnScrollViewIsScrollingBroadcasted(scrolling);
}

- (void)broadcastScrollViewIsZooming:(BOOL)zooming {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
  self.observer->OnScrollViewIsZoomingBroadcasted(zooming);
}

- (void)broadcastScrollViewIsDragging:(BOOL)dragging {
  CHECK(base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault));
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
