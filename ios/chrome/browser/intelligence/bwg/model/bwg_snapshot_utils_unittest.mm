// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_snapshot_utils.h"

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for bwg_snapshot_utils.
class BwgSnapshotUtilsTest : public PlatformTest {
 public:
  BwgSnapshotUtilsTest() = default;
};

// Tests that GetCroppedFullscreenSnapshot returns nil for a nil view.
TEST_F(BwgSnapshotUtilsTest, ReturnsNilForNilView) {
  EXPECT_EQ(nil, bwg_snapshot_utils::GetCroppedFullscreenSnapshot(nil));
}

// Tests that GetCroppedFullscreenSnapshot returns nil for a view without a
// window.
TEST_F(BwgSnapshotUtilsTest, ReturnsNilForViewWithoutWindow) {
  UIView* view_without_window = [[UIView alloc] init];
  EXPECT_EQ(nil, bwg_snapshot_utils::GetCroppedFullscreenSnapshot(
                     view_without_window));
}

// Tests that GetCroppedFullscreenSnapshot returns nil for a view without a
// content area guide.
TEST_F(BwgSnapshotUtilsTest, ReturnsNilForViewWithoutContentAreaGuide) {
  // Set up window and view.
  UIWindow* window =
      [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 300, 500)];
  UIView* view = [[UIView alloc] initWithFrame:window.bounds];
  [window addSubview:view];
  [window makeKeyAndVisible];

  EXPECT_EQ(nil, bwg_snapshot_utils::GetCroppedFullscreenSnapshot(view));
}

// Tests a successful snapshot and crop by making sure the result is not nil and
// has the correct dimensions.
TEST_F(BwgSnapshotUtilsTest, SuccessfulSnapshotAndCrop) {
  // Set up window and view.
  CGFloat window_width = 300;
  CGFloat window_height = 500;
  UIWindow* window = [[UIWindow alloc]
      initWithFrame:CGRectMake(0, 0, window_width, window_height)];
  UIView* view = [[UIView alloc] initWithFrame:window.bounds];
  [window addSubview:view];
  [window makeKeyAndVisible];

  // Set up content area guide.
  CGFloat content_area_top_offset = 50;
  [view addLayoutGuide:[[NamedGuide alloc] initWithName:kContentAreaGuide]];
  NamedGuide* content_area_layout_guide =
      [NamedGuide guideWithName:kContentAreaGuide view:view];
  [content_area_layout_guide.topAnchor
      constraintEqualToAnchor:view.topAnchor
                     constant:content_area_top_offset]
      .active = YES;
  [window layoutIfNeeded];

  UIImage* cropped_snapshot =
      bwg_snapshot_utils::GetCroppedFullscreenSnapshot(view);
  EXPECT_NE(nil, cropped_snapshot);

  CGFloat scale = window.screen.scale;
  CGSize expected_size_in_pixels = CGSizeMake(
      window_width * scale, (window_height - content_area_top_offset) * scale);

  EXPECT_EQ(expected_size_in_pixels.width, cropped_snapshot.size.width);
  EXPECT_EQ(expected_size_in_pixels.height, cropped_snapshot.size.height);
}
