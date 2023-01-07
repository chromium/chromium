// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"

#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test implementation of ChromeBroadcastObserverInterface.
class TestChromeBroadcastObserver : public ChromeBroadcastObserverInterface {
 public:
  // Received broadcast values.
  CGFloat scroll_offset() const { return scroll_offset_; }
  bool scroll_view_scrolling() const { return scroll_view_scrolling_; }
  bool scroll_view_dragging() const { return scroll_view_dragging_; }
  CGFloat collapsed_height() const { return collapsed_height_; }
  CGFloat expanded_height() const { return expanded_height_; }

 private:
  // ChromeBroadcastObserverInterface:
  void OnContentScrollOffsetBroadcasted(CGFloat offset) override {
    scroll_offset_ = offset;
  }
  void OnScrollViewIsScrollingBroadcasted(bool scrolling) override {
    scroll_view_scrolling_ = scrolling;
  }
  void OnScrollViewIsDraggingBroadcasted(bool dragging) override {
    scroll_view_dragging_ = dragging;
  }
  void OnCollapsedToolbarHeightBroadcasted(CGFloat height) override {
    collapsed_height_ = height;
    ;
  }
  void OnExpandedToolbarHeightBroadcasted(CGFloat height) override {
    expanded_height_ = height;
  }

  CGFloat scroll_offset_ = 0.0;
  bool scroll_view_scrolling_ = false;
  bool scroll_view_dragging_ = false;
  CGFloat collapsed_height_ = 0.0;
  CGFloat expanded_height_ = 0.0;
};

// Test fixture for ChromeBroadcastOberverBridge.
class ChromeBroadcastObserverBridgeTest : public PlatformTest {
 public:
  ChromeBroadcastObserverBridgeTest()
      : PlatformTest(),
        bridge_([[ChromeBroadcastOberverBridge alloc]
            initWithObserver:&observer_]) {}

  const TestChromeBroadcastObserver& observer() { return observer_; }
  id<ChromeBroadcastObserver> bridge() { return bridge_; }

 private:
  TestChromeBroadcastObserver observer_;
  __strong ChromeBroadcastOberverBridge* bridge_ = nil;
};

// Tests that `-broadcastContentScrollOffset:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, ContentOffset) {
  ASSERT_EQ(observer().scroll_offset(), 0.0);
  const CGFloat kOffset = 50.0;
  [bridge() broadcastContentScrollOffset:kOffset];
  EXPECT_EQ(observer().scroll_offset(), kOffset);
}

// Tests that `-broadcastScrollViewIsScrolling:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, ScrollViewIsScrolling) {
  ASSERT_FALSE(observer().scroll_view_scrolling());
  [bridge() broadcastScrollViewIsScrolling:YES];
  EXPECT_TRUE(observer().scroll_view_scrolling());
}

// Tests that `-broadcastScrollViewIsDragging:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, ScrollViewIsDragging) {
  ASSERT_FALSE(observer().scroll_view_dragging());
  [bridge() broadcastScrollViewIsDragging:YES];
  EXPECT_TRUE(observer().scroll_view_dragging());
}

// Tests that `-broadcastToolbarHeight:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, ToolbarHeight) {
  ASSERT_EQ(observer().collapsed_height(), 0.0);
  ASSERT_EQ(observer().expanded_height(), 0.0);
  const CGFloat kCollapsedHeight = 10.0;
  const CGFloat kExpandedHeight = 50.0;
  [bridge() broadcastCollapsedToolbarHeight:kCollapsedHeight];
  [bridge() broadcastExpandedToolbarHeight:kExpandedHeight];
  EXPECT_EQ(observer().collapsed_height(), kCollapsedHeight);
  EXPECT_EQ(observer().expanded_height(), kExpandedHeight);
}
