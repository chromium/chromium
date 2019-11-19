// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/ui/page_display_state.h"

#include "testing/gtest/include/gtest/gtest.h"

#define EXPECT_NAN(value) EXPECT_NE(value, value)
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PageDisplayStateTest = PlatformTest;

// Tests that the empty constructor creates an invalid PageDisplayState with all
// NAN values.
TEST_F(PageDisplayStateTest, EmptyConstructor) {
  web::PageDisplayState state;
  EXPECT_NAN(state.scroll_state().content_offset().y);
  EXPECT_NAN(state.scroll_state().content_offset().x);
  EXPECT_NAN(state.scroll_state().content_inset().top);
  EXPECT_NAN(state.scroll_state().content_inset().left);
  EXPECT_NAN(state.scroll_state().content_inset().bottom);
  EXPECT_NAN(state.scroll_state().content_inset().right);
  EXPECT_NAN(state.zoom_state().minimum_zoom_scale());
  EXPECT_NAN(state.zoom_state().maximum_zoom_scale());
  EXPECT_NAN(state.zoom_state().zoom_scale());
  EXPECT_FALSE(state.IsValid());
}

// Tests that the constructor with input states correctly populates the display
// state.
TEST_F(PageDisplayStateTest, StatesConstructor) {
  const CGPoint kContentOffset = CGPointMake(0.0, 1.0);
  const UIEdgeInsets kContentInset = UIEdgeInsetsMake(0.0, 1.0, 2.0, 3.0);
  web::PageScrollState scroll_state(kContentOffset, kContentInset);
  EXPECT_TRUE(
      CGPointEqualToPoint(scroll_state.content_offset(), kContentOffset));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(scroll_state.content_inset(),
                                            kContentInset));
  EXPECT_TRUE(scroll_state.IsValid());
  web::PageZoomState zoom_state(1.0, 5.0, 1.0);
  EXPECT_EQ(1.0, zoom_state.minimum_zoom_scale());
  EXPECT_EQ(5.0, zoom_state.maximum_zoom_scale());
  EXPECT_EQ(1.0, zoom_state.zoom_scale());
  EXPECT_TRUE(zoom_state.IsValid());
  web::PageDisplayState state(scroll_state, zoom_state);
  EXPECT_EQ(scroll_state, state.scroll_state());
  EXPECT_EQ(zoom_state, state.zoom_state());
  EXPECT_TRUE(state.IsValid());
}

// Tests converting between a PageDisplayState, its serialization, and back.
TEST_F(PageDisplayStateTest, Serialization) {
  web::PageDisplayState state(CGPointMake(0.0, 1.0),
                              UIEdgeInsetsMake(0.0, 1.0, 2.0, 3.0), 1.0, 5.0,
                              1.0);
  web::PageDisplayState new_state(state.GetSerialization());
  EXPECT_EQ(state, new_state);
}

// Tests that the PageScrollState is updated correctly when restored from the
// deprecated serialization keys.
// TODO(crbug.com/926041): Delete this test when legacy keys are removed.
TEST_F(PageDisplayStateTest, LegacySerialization) {
  const CGPoint kContentOffset = CGPointMake(25.0, 100.0);
  web::PageDisplayState state(
      @{@"scrollX" : @(kContentOffset.x),
        @"scrollY" : @(kContentOffset.y)});
  EXPECT_TRUE(CGPointEqualToPoint(kContentOffset,
                                  state.scroll_state().content_offset()));
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(
      state.scroll_state().content_inset(), UIEdgeInsetsZero));
}

// Tests PageScrollState::GetEffectiveContentOffsetForContentInset().
TEST_F(PageDisplayStateTest, EffectiveContentOffset) {
  // kContentOffset is chosen such that a page with kTopInset is scrolled to the
  // top.
  const CGFloat kTopInset = 100;
  const CGPoint kContentOffset = CGPointMake(0.0, -kTopInset);
  const UIEdgeInsets kContentInset = UIEdgeInsetsMake(kTopInset, 0.0, 0.0, 0.0);
  web::PageScrollState scroll_state(kContentOffset, kContentInset);
  // Tests that GetEffectiveContentOffsetForContentInset() returns the scrolled-
  // to-top content offset for kNewTopInset.
  const CGFloat kNewTopInset = 50.0;
  const UIEdgeInsets kNewContentInset =
      UIEdgeInsetsMake(kNewTopInset, 0.0, 0.0, 0.0);
  CGPoint effective_content_offset =
      scroll_state.GetEffectiveContentOffsetForContentInset(kNewContentInset);
  EXPECT_EQ(effective_content_offset.y, -kNewTopInset);
}
