// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"

#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "testing/platform_test.h"

// Test implementation of ChromeBroadcastObserverInterface.
class TestChromeBroadcastObserver : public ChromeBroadcastObserverInterface {
 public:
  // Received broadcast values.
  CGFloat scroll_offset() const { return scroll_offset_; }
  bool scroll_view_scrolling() const { return scroll_view_scrolling_; }
  bool scroll_view_dragging() const { return scroll_view_dragging_; }
  CGFloat collapsed_top_toolbar_height() const {
    return collapsed_top_toolbar_height_;
  }
  CGFloat expanded_top_toolbar_height() const {
    return expanded_top_toolbar_height_;
  }
  CGFloat collapsed_bottom_toolbar_height() const {
    return collapsed_bottom_toolbar_height_;
  }
  CGFloat expanded_bottom_toolbar_height() const {
    return expanded_bottom_toolbar_height_;
  }

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
  void OnCollapsedTopToolbarHeightBroadcasted(CGFloat height) override {
    collapsed_top_toolbar_height_ = height;
  }
  void OnExpandedTopToolbarHeightBroadcasted(CGFloat height) override {
    expanded_top_toolbar_height_ = height;
  }
  void OnCollapsedBottomToolbarHeightBroadcasted(CGFloat height) override {
    collapsed_bottom_toolbar_height_ = height;
  }
  void OnExpandedBottomToolbarHeightBroadcasted(CGFloat height) override {
    expanded_bottom_toolbar_height_ = height;
  }

  CGFloat scroll_offset_ = 0.0;
  bool scroll_view_scrolling_ = false;
  bool scroll_view_dragging_ = false;
  CGFloat collapsed_top_toolbar_height_ = 0.0;
  CGFloat expanded_top_toolbar_height_ = 0.0;
  CGFloat collapsed_bottom_toolbar_height_ = 0.0;
  CGFloat expanded_bottom_toolbar_height_ = 0.0;
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kSmoothScrollingDefault);
  ASSERT_FALSE(observer().scroll_view_scrolling());
  [bridge() broadcastScrollViewIsScrolling:YES];
  EXPECT_TRUE(observer().scroll_view_scrolling());
}

// Tests that `-broadcastScrollViewIsDragging:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, ScrollViewIsDragging) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kSmoothScrollingDefault);

  ASSERT_FALSE(observer().scroll_view_dragging());
  [bridge() broadcastScrollViewIsDragging:YES];
  EXPECT_TRUE(observer().scroll_view_dragging());
}

// Tests that `-broadcastTopToolbarHeight:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, TopToolbarHeight) {
  ASSERT_EQ(observer().collapsed_top_toolbar_height(), 0.0);
  ASSERT_EQ(observer().expanded_top_toolbar_height(), 0.0);
  const CGFloat kCollapsedHeight = 10.0;
  const CGFloat kExpandedHeight = 50.0;
  [bridge() broadcastCollapsedTopToolbarHeight:kCollapsedHeight];
  [bridge() broadcastExpandedTopToolbarHeight:kExpandedHeight];
  EXPECT_EQ(observer().collapsed_top_toolbar_height(), kCollapsedHeight);
  EXPECT_EQ(observer().expanded_top_toolbar_height(), kExpandedHeight);
}

// Tests that `-broadcastBottomToolbarHeight:` is correctly forwarded to the
// observer.
TEST_F(ChromeBroadcastObserverBridgeTest, BottomToolbarHeight) {
  ASSERT_EQ(observer().collapsed_bottom_toolbar_height(), 0.0);
  ASSERT_EQ(observer().expanded_bottom_toolbar_height(), 0.0);
  const CGFloat kCollapsedHeight = 30.0;
  const CGFloat kExpandedHeight = 60.0;
  [bridge() broadcastCollapsedBottomToolbarHeight:kCollapsedHeight];
  [bridge() broadcastExpandedBottomToolbarHeight:kExpandedHeight];
  EXPECT_EQ(observer().collapsed_bottom_toolbar_height(), kCollapsedHeight);
  EXPECT_EQ(observer().expanded_bottom_toolbar_height(), kExpandedHeight);
}
