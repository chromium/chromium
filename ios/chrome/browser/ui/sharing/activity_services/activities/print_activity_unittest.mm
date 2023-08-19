// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/print_activity.h"

#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Test fixture for covering the PrintActivity class.
class PrintActivityTest : public PlatformTest {
 protected:
  PrintActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  }

  // Creates a ShareToData instance with `is_page_printable` set.
  ShareToData* CreateData(bool is_page_printable) {
    return [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                      visibleURL:GURL("https://google.com/")
                                           title:@"Some Title"
                                  additionalText:nil
                                 isOriginalTitle:YES
                                 isPagePrintable:is_page_printable
                                isPageSearchable:YES
                                canSendTabToSelf:YES
                                       userAgent:web::UserAgentType::MOBILE
                              thumbnailGenerator:nil
                                    linkMetadata:nil];
  }

  id mocked_handler_;
};

// Tests that the activity can be performed when the data object shows that the
// page is printable.
TEST_F(PrintActivityTest, DataTrue_ActivityEnabled) {
  ShareToData* data = CreateData(true);
  PrintActivity* activity = [[PrintActivity alloc] initWithData:data
                                                        handler:mocked_handler_
                                             baseViewController:nil];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the data object shows that
// the page is not printable.
TEST_F(PrintActivityTest, DataFalse_ActivityDisabled) {
  ShareToData* data = CreateData(false);
  PrintActivity* activity = [[PrintActivity alloc] initWithData:data
                                                        handler:mocked_handler_
                                             baseViewController:nil];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity can be performed when the data object is an image.
TEST_F(PrintActivityTest, DataTrue_Image) {
  UIImage* redImage = ImageWithColor([UIColor redColor]);
  ShareImageData* data = [[ShareImageData alloc] initWithImage:redImage
                                                         title:@"title"];
  PrintActivity* activity =
      [[PrintActivity alloc] initWithImageData:data
                                       handler:mocked_handler_
                            baseViewController:nil];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the data object is a nil
// image.
TEST_F(PrintActivityTest, DataFalse_ImageNil) {
  ShareImageData* data = [[ShareImageData alloc] initWithImage:nil
                                                         title:@"title"];
  PrintActivity* activity =
      [[PrintActivity alloc] initWithImageData:data
                                       handler:mocked_handler_
                            baseViewController:nil];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity triggers the right handler method to print
// a tab.
TEST_F(PrintActivityTest, ExecuteActivity_CallsHandler) {
  [[mocked_handler_ expect] printTabWithBaseViewController:nil];

  ShareToData* data = CreateData(true);
  PrintActivity* activity = [[PrintActivity alloc] initWithData:data
                                                        handler:mocked_handler_
                                             baseViewController:nil];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}

// Tests that executing the activity triggers the right handler method to print
// an image.
TEST_F(PrintActivityTest, ExecuteActivity_CallsImageHandler) {
  UIImage* redImage = ImageWithColor([UIColor redColor]);
  NSString* title = @"title";
  ShareImageData* data = [[ShareImageData alloc] initWithImage:redImage
                                                         title:title];

  [[mocked_handler_ expect] printImage:redImage
                                 title:title
                    baseViewController:nil];

  PrintActivity* activity =
      [[PrintActivity alloc] initWithImageData:data
                                       handler:mocked_handler_
                            baseViewController:nil];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
