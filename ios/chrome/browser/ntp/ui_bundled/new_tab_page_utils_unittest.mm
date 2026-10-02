// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"

#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_item.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_config.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class NewTabPageUtilsTest : public PlatformTest {};

// Tests that CreateMostVisitedContainerView creates the expected container
// hierarchy and styling.
TEST_F(NewTabPageUtilsTest, TestCreateMostVisitedContainerView) {
  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedItem* item = [[MostVisitedItem alloc] init];
  config.mostVisitedItems = @[ item ];

  MostVisitedTilesCollectionView* collection_view =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  UIView* container = CreateMostVisitedContainerView(collection_view, YES);
  ASSERT_TRUE(container != nil);
  EXPECT_TRUE(container.clipsToBounds);
  EXPECT_FLOAT_EQ(24.0, container.layer.cornerRadius);

  BOOL has_card_background = NO;
  BOOL has_collection_view = NO;
  for (UIView* subview in container.subviews) {
    if ([subview isKindOfClass:[NTPCardBackgroundView class]]) {
      has_card_background = YES;
    }
    if (subview == collection_view) {
      has_collection_view = YES;
    }
  }
  EXPECT_TRUE(has_card_background);
  EXPECT_TRUE(has_collection_view);
}

// Tests that CreateMostVisitedContainerView without background does not include
// NTPCardBackgroundView.
TEST_F(NewTabPageUtilsTest, TestCreateMostVisitedContainerViewNoBackground) {
  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedTilesCollectionView* collection_view =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  UIView* container = CreateMostVisitedContainerView(collection_view, NO);
  ASSERT_TRUE(container != nil);

  BOOL has_card_background = NO;
  for (UIView* subview in container.subviews) {
    if ([subview isKindOfClass:[NTPCardBackgroundView class]]) {
      has_card_background = YES;
    }
  }
  EXPECT_FALSE(has_card_background);
}

// Tests that MostVisitedContainerHeight returns collectionView contentSize plus
// bottom padding when the collection view has positive contentSize.
TEST_F(NewTabPageUtilsTest,
       TestMostVisitedContainerHeightWithCollectionViewContentSize) {
  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedTilesCollectionView* collection_view =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  constexpr CGFloat kContentHeight = 91.33;
  constexpr CGFloat kExpectedTotalHeight = 101.33;
  [collection_view setContentSize:CGSizeMake(390, kContentHeight)];

  EXPECT_FLOAT_EQ(kExpectedTotalHeight,
                  MostVisitedContainerHeight(collection_view, nil));
}

// Tests that MostVisitedContainerHeight uses fitting size when collectionView
// is nil.
TEST_F(NewTabPageUtilsTest, TestMostVisitedContainerHeightWithFittingFallback) {
  UIView* content = [[UIView alloc] init];
  content.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [content.heightAnchor constraintEqualToConstant:120.0],
    [content.widthAnchor constraintEqualToConstant:300.0],
  ]];

  EXPECT_FLOAT_EQ(120.0, MostVisitedContainerHeight(nil, content));
}

// Tests that MostVisitedContainerHeight prioritizes collectionView contentSize
// over fallbackView fitting size.
TEST_F(NewTabPageUtilsTest,
       TestMostVisitedContainerHeightPrioritizesCollectionViewOverFallback) {
  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedTilesCollectionView* collection_view =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];
  [collection_view setContentSize:CGSizeMake(390, 100.0)];

  UIView* fallback = [[UIView alloc] init];
  fallback.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [fallback.heightAnchor constraintEqualToConstant:64.0],
    [fallback.widthAnchor constraintEqualToConstant:300.0],
  ]];

  EXPECT_FLOAT_EQ(110.0, MostVisitedContainerHeight(collection_view, fallback));
}

// Tests that MostVisitedContainerHeight returns 0 when both arguments are nil.
TEST_F(NewTabPageUtilsTest, TestMostVisitedContainerHeightNil) {
  EXPECT_FLOAT_EQ(0.0, MostVisitedContainerHeight(nil, nil));
}

// Tests that MostVisitedContainerHeight falls back to collectionView
// intrinsicContentSize plus bottom padding when contentSize is zero.
TEST_F(NewTabPageUtilsTest,
       TestMostVisitedContainerHeightFallbackToIntrinsicContentSize) {
  MostVisitedTilesConfig* config =
      [[MostVisitedTilesConfig alloc] initWithLayoutGuideCenter:nil];
  MostVisitedTilesCollectionView* collection_view =
      [[MostVisitedTilesCollectionView alloc] initWithConfig:config];

  CGFloat intrinsic_height = [collection_view intrinsicContentSize].height;
  ASSERT_GT(intrinsic_height, 0.0);

  constexpr CGFloat kContainerBottomPadding = 10.0;
  CGFloat expected_total_height = intrinsic_height + kContainerBottomPadding;
  EXPECT_FLOAT_EQ(expected_total_height,
                  MostVisitedContainerHeight(collection_view, nil));
}

// Tests that CreateMostVisitedContainerView returns nil when collectionView is
// nil.
TEST_F(NewTabPageUtilsTest,
       TestCreateMostVisitedContainerViewNilCollectionView) {
  EXPECT_EQ(nil, CreateMostVisitedContainerView(nil, YES));
  EXPECT_EQ(nil, CreateMostVisitedContainerView(nil, NO));
}
