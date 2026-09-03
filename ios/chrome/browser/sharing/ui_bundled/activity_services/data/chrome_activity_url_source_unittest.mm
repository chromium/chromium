// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_url_source.h"

#import <LinkPresentation/LinkPresentation.h>
#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using ui::test::uiimage_utils::UIImageWithSizeAndSolidColorAndScale;

using ChromeActivityURLSourceTest = PlatformTest;

// Tests that the thumbnail is resized to suggested size if thumbnail image
// exists.
TEST_F(ChromeActivityURLSourceTest, ThumbnailImageForActivityType_WithSize) {
  NSURL* url = [NSURL URLWithString:@"https://example.com"];
  ChromeActivityURLSource* source =
      [[ChromeActivityURLSource alloc] initWithShareURL:url
                                                subject:@"Test Subject"];

  const CGSize image_size = CGSizeMake(100.0, 100.0);
  const CGFloat scale = 2.0;
  source.thumbnail = UIImageWithSizeAndSolidColorAndScale(
      image_size, [UIColor whiteColor], scale);

  UIActivityViewController* activity_view_controller =
      [[UIActivityViewController alloc] initWithActivityItems:@[]
                                        applicationActivities:nil];
  const CGSize suggested_size = CGSizeMake(50.0, 50.0);
  UIImage* result = [source activityViewController:activity_view_controller
                     thumbnailImageForActivityType:UIActivityTypeMail
                                     suggestedSize:suggested_size];

  ASSERT_TRUE(result);
  EXPECT_EQ(suggested_size.width, result.size.width);
  EXPECT_EQ(suggested_size.height, result.size.height);
}

// Tests that thumbnailImageForActivityType returns nil when thumbnail is nil.
TEST_F(ChromeActivityURLSourceTest,
       ThumbnailImageForActivityType_NilThumbnail) {
  NSURL* url = [NSURL URLWithString:@"https://example.com"];
  ChromeActivityURLSource* source =
      [[ChromeActivityURLSource alloc] initWithShareURL:url
                                                subject:@"Test Subject"];
  source.thumbnail = nil;

  UIActivityViewController* activity_view_controller =
      [[UIActivityViewController alloc] initWithActivityItems:@[]
                                        applicationActivities:nil];
  UIImage* result = [source activityViewController:activity_view_controller
                     thumbnailImageForActivityType:UIActivityTypeMail
                                     suggestedSize:CGSizeMake(50.0, 50.0)];

  EXPECT_FALSE(result);
}
