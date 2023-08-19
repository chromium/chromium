// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"

#import "testing/platform_test.h"

// Fixture for BrowserContainerViewController testing.
class BrowserContainerViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    view_controller_ = [[BrowserContainerViewController alloc] init];
    ASSERT_TRUE(view_controller_);
    content_view_ = [[UIView alloc] init];
    ASSERT_TRUE(content_view_);
    content_view_controller_ = [[UIViewController alloc] init];
    ASSERT_TRUE(content_view_controller_);
  }
  BrowserContainerViewController* view_controller_;
  UIView* content_view_;
  UIViewController* content_view_controller_;
};

// Tests adding a new content view when BrowserContainerViewController does not
// currently have a content view.
TEST_F(BrowserContainerViewControllerTest, AddingContentView) {
  ASSERT_FALSE([content_view_ superview]);

  view_controller_.contentView = content_view_;
  EXPECT_EQ(view_controller_.view, content_view_.superview);
}

// Tests adding a new content view when BrowserContainerViewController does not
// currently have a content view controller.
TEST_F(BrowserContainerViewControllerTest, AddingContentViewController) {
  ASSERT_FALSE([content_view_controller_.view superview]);

  view_controller_.contentViewController = content_view_controller_;
  EXPECT_EQ(view_controller_.view, content_view_controller_.view.superview);
}

// Tests removing previously added content view.
TEST_F(BrowserContainerViewControllerTest, RemovingContentView) {
  view_controller_.contentView = content_view_;
  ASSERT_EQ(view_controller_.view, content_view_.superview);

  view_controller_.contentView = nil;
  EXPECT_FALSE([content_view_ superview]);
}

// Tests removing previously added content view controller.
TEST_F(BrowserContainerViewControllerTest, RemovingContentViewController) {
  view_controller_.contentViewController = content_view_controller_;
  ASSERT_EQ(view_controller_.view, content_view_controller_.view.superview);

  view_controller_.contentViewController = nil;
  EXPECT_FALSE([content_view_controller_.view superview]);
}

// Tests adding a new content view when BrowserContainerViewController already
// has a content view.
TEST_F(BrowserContainerViewControllerTest, ReplacingContentView) {
  view_controller_.contentView = content_view_;
  ASSERT_EQ(view_controller_.view, content_view_.superview);

  UIView* content_view2 = [[UIView alloc] init];
  view_controller_.contentView = content_view2;
  EXPECT_FALSE([content_view_ superview]);
  EXPECT_EQ(view_controller_.view, content_view2.superview);
}

// Tests that BrowserContainerViewController contentViews and
// contentViewControllers are always added at index zero, with the
// contentViewControllers above the contentView.
TEST_F(BrowserContainerViewControllerTest, ContentViewIndex) {
  view_controller_.contentView = content_view_;
  ASSERT_EQ(view_controller_.view, content_view_.superview);

  [view_controller_.view addSubview:[[UIView alloc] init]];

  UIView* content_view2 = [[UIView alloc] init];
  view_controller_.contentView = content_view2;
  EXPECT_EQ([view_controller_.view.subviews indexOfObject:content_view2],
            static_cast<NSUInteger>(0));

  UIViewController* content_view_controller2 = [[UIViewController alloc] init];
  view_controller_.contentViewController = content_view_controller2;
  EXPECT_EQ([view_controller_.view.subviews
                indexOfObject:content_view_controller2.view],
            static_cast<NSUInteger>(1));
}

// Tests adding a new content view controller when
// BrowserContainerViewController already has a content view or a content view
// controller.
TEST_F(BrowserContainerViewControllerTest, ReplacingContentViewController) {
  view_controller_.contentView = content_view_;
  ASSERT_EQ(view_controller_.view, content_view_.superview);

  UIViewController* content_view_controller2 = [[UIViewController alloc] init];
  view_controller_.contentViewController = content_view_controller2;
  EXPECT_TRUE([content_view_ superview]);
  EXPECT_EQ(view_controller_.view, content_view_controller2.view.superview);

  UIView* content_view2 = [[UIView alloc] init];
  view_controller_.contentView = content_view2;
  EXPECT_FALSE([content_view_ superview]);
  EXPECT_FALSE([content_view_controller2.view superview]);
  EXPECT_EQ(view_controller_.view, content_view2.superview);
}
