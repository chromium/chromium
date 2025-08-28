// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"

// #import "base/test/task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
// #import "third_party/ocmock/OCMock/OCMock.h"
// #import "third_party/ocmock/gtest_support.h"

class HomeCustomizationImageViewTest : public PlatformTest {
 protected:
  void ExpectRectEqual(CGRect expectedRect, CGRect actualRect) {
    EXPECT_FLOAT_EQ(expectedRect.origin.x, actualRect.origin.x);
    EXPECT_FLOAT_EQ(expectedRect.origin.y, actualRect.origin.y);
    EXPECT_FLOAT_EQ(expectedRect.size.width, actualRect.size.width);
    EXPECT_FLOAT_EQ(expectedRect.size.height, actualRect.size.height);
  }

  CGSize image_size_ = CGSizeMake(200, 200);

  CGRect image_bounds_ =
      CGRectMake(0, 0, image_size_.width, image_size_.height);
};

// Tests that the frame does not change if the orientation of the frame matches
// the orientation of the view.
TEST_F(HomeCustomizationImageViewTest,
       UpdateDesiredFrameNoChangeIfOrientationMatches) {
  CGRect frame = CGRectMake(25, 25, 50, 50);
  CGRect updated_frame = UpdateDesiredFrame(frame, YES, image_size_);
  ExpectRectEqual(frame, updated_frame);
}

// Tests that the frame rotates if the orientation of the frame does not match
// the orientation of the view.
TEST_F(HomeCustomizationImageViewTest, UpdateDesiredFrameRotates) {
  CGRect frame = CGRectMake(70, 75, 60, 50);
  CGRect rotated_frame = CGRectMake(75, 70, 50, 60);
  CGRect updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);

  frame = CGRectMake(100, 0, 20, 40);
  rotated_frame = CGRectMake(90, 10, 40, 20);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
}

// Tests that if the frame rotates and extends outside the bounds of the image,
// it gets shrunk until it is within the bounds of the image.
TEST_F(HomeCustomizationImageViewTest,
       UpdateDesiredFrameShrinksHorizontallyAfterRotation) {
  CGRect frame = CGRectMake(0, 0, 50, 100);
  // Rotated rect becomes (-25, 25, 100, 50). So shrinking should shrink 25
  // units horizontally (on each side) and 12.5 units vertically.
  CGRect rotated_frame = CGRectMake(0, 37.5, 50, 25);
  CGRect updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the upper right corner of the image.
  frame = CGRectMake(150, 0, 50, 100);
  // Rotated rect becomes (125, 25, 100, 50), which extends over the right
  // border of the image by 25. So shrinking should shrink 25 units horizontally
  // (on each side) and 12.5 units vertically.
  rotated_frame = CGRectMake(150, 37.5, 50, 25);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the lower left corner of the image.
  frame = CGRectMake(0, 100, 50, 100);
  // Rotated rect becomes (-25, 125, 100, 50). So shrinking should shrink 25
  // units horizontally (on each side) and 12.5 units vertically.
  rotated_frame = CGRectMake(0, 137.5, 50, 25);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the lower right corner of the image.
  frame = CGRectMake(150, 100, 50, 100);
  // Rotated rect becomes (125, 125, 100, 50), which extends over the right
  // border of the image by 25. So shrinking should shrink 25 units horizontally
  // (on each side) and 12.5 units vertically.
  rotated_frame = CGRectMake(150, 137.5, 50, 25);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));
}

TEST_F(HomeCustomizationImageViewTest,
       UpdateDesiredFrameShrinksVerticallyAfterRotation) {
  CGRect frame = CGRectMake(0, 0, 100, 50);
  // Rotated rect becomes (25, -25, 50, 100). So shrinking should shrink 25
  // units vertically (on each side) and 12.5 units horizontally.
  CGRect rotated_frame = CGRectMake(37.5, 0, 25, 50);
  CGRect updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the upper right corner of the image.
  frame = CGRectMake(100, 0, 100, 50);
  // Rotated rect becomes (125, -25, 50, 100) So shrinking should shrink 25
  // units vertically (on each side) and 12.5 units horizontally.
  rotated_frame = CGRectMake(137.5, 0, 25, 50);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the lower left corner of the image.
  frame = CGRectMake(0, 150, 100, 50);
  // Rotated rect becomes (25, 125, 50, 100), which extends over the bottom
  // border of the image by 25. So shrinking should shrink 25 units vertically
  // (on each side) and 12.5 units horizontally.
  rotated_frame = CGRectMake(37.5, 150, 25, 50);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));

  // Do the same test in the lower right corner of the image.
  frame = CGRectMake(100, 150, 100, 50);
  // Rotated rect becomes (125, 125, 50, 100), which extends over the bottm
  // border of the image by 25. So shrinking should shrink 25 units vertically
  // (on each side) and 12.5 units horizontally.
  rotated_frame = CGRectMake(137.5, 150, 25, 50);
  updated_frame = UpdateDesiredFrame(frame, NO, image_size_);
  ExpectRectEqual(rotated_frame, updated_frame);
  EXPECT_TRUE(CGRectContainsRect(image_bounds_, updated_frame));
}
