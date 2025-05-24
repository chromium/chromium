// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/js_image_transcoder/java_script_image_transcoder.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "base/test/ios/wait_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

class JavaScriptImageTranscoderTest : public PlatformTest {
 public:
  JavaScriptImageTranscoderTest() : PlatformTest() {
    transcoder_ = std::make_unique<web::JavaScriptImageTranscoder>();
  }

 protected:
  std::unique_ptr<web::JavaScriptImageTranscoder> transcoder_;
};

TEST_F(JavaScriptImageTranscoderTest, TranscodesFromPNGToPNG) {
  UIImage* source_image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(64, 64), [UIColor whiteColor]);
  NSData* source_image_data = UIImagePNGRepresentation(source_image);
  __block NSData* destination_image_data = nil;
  transcoder_->TranscodeImage(source_image_data, @"image/png", nil, nil, nil,
                              base::BindOnce(^(NSData* result, NSError* error) {
                                destination_image_data = result;
                              }));
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return destination_image_data != nil;
      }));
  UIImage* destination_image = [UIImage imageWithData:destination_image_data];
  // The PNG representation of the source and destination differ.
  EXPECT_NSNE(source_image_data, destination_image_data);
  EXPECT_EQ(source_image.size.width, destination_image.size.width);
  EXPECT_EQ(source_image.size.height, destination_image.size.height);
}

TEST_F(JavaScriptImageTranscoderTest, TranscodesFromPNGToPNGWithResize) {
  UIImage* source_image = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(64, 64), [UIColor whiteColor]);
  NSData* source_image_data = UIImagePNGRepresentation(source_image);
  __block NSData* destination_image_data = nil;
  transcoder_->TranscodeImage(source_image_data, @"image/png", @(32), @(32),
                              nil,
                              base::BindOnce(^(NSData* result, NSError* error) {
                                destination_image_data = result;
                              }));
  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^{
        return destination_image_data != nil;
      }));
  UIImage* destination_image = [UIImage imageWithData:destination_image_data];
  EXPECT_EQ(32, destination_image.size.width);
  EXPECT_EQ(32, destination_image.size.height);
}
