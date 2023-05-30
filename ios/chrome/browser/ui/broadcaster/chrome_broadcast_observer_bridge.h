// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_BRIDGE_H_

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"

// Interface for C++ objects that care about broadcasted UI state.
class ChromeBroadcastObserverInterface {
 public:
  virtual ~ChromeBroadcastObserverInterface();

  // Invoked by `-broadcastScrollViewSize:`.
  virtual void OnScrollViewSizeBroadcasted(CGSize scroll_view_size) {}

  // Invoked by `-broadcastScrollViewContentSize:`.
  virtual void OnScrollViewContentSizeBroadcasted(CGSize content_size) {}

  // Invoked by `-broadcastScrollViewContentInset:`.
  virtual void OnScrollViewContentInsetBroadcasted(UIEdgeInsets conent_inset) {}

  // Invoked by `-broadcastContentScrollOffset:`.
  virtual void OnContentScrollOffsetBroadcasted(CGFloat offset) {}

  // Invoked by `-broadcastScrollViewIsScrolling:`.
  virtual void OnScrollViewIsScrollingBroadcasted(bool scrolling) {}

  // Invoked by `-broadcastScrollViewIsZooming:`.
  virtual void OnScrollViewIsZoomingBroadcasted(bool zooming) {}

  // Invoked by `-broadcastScrollViewIsDragging:`.
  virtual void OnScrollViewIsDraggingBroadcasted(bool dragging) {}

  // Invoked by `-broadcastCollapsedTopToolbarHeight:`.
  virtual void OnCollapsedTopToolbarHeightBroadcasted(CGFloat height) {}

  // Invoked by `-broadcastExpandedTopToolbarHeight:`.
  virtual void OnExpandedTopToolbarHeightBroadcasted(CGFloat height) {}

  // Invoked by `-broadcastExpandedBottomToolbarHeight:`.
  virtual void OnExpandedBottomToolbarHeightBroadcasted(CGFloat height) {}

  // Invoked by `-broadcastCollapsedBottomToolbarHeight:`.
  virtual void OnCollapsedBottomToolbarHeightBroadcasted(CGFloat height) {}
};

// Bridge object that forwards broadcasted UI state to objects that subclass
// ChromeBroadcastObserverInterface.
@interface ChromeBroadcastOberverBridge : NSObject<ChromeBroadcastObserver>

// The observer being updated.
@property(nonatomic, readonly, nonnull)
    ChromeBroadcastObserverInterface* observer;

// Initializer for a bridge that updates `observer`.
- (nullable instancetype)initWithObserver:
    (nonnull ChromeBroadcastObserverInterface*)observer
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_BRIDGE_H_
