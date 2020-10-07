// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"

#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#include "ios/chrome/browser/ui/commands/browser_commands.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for covering the PrintActivity class.
class PrintActivityTest : public PlatformTest {
 protected:
  PrintActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(BrowserCommands));
  }

  // Creates a ShareToData instance with |is_page_printable| set.
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
                              thumbnailGenerator:nil];
  }

  id mocked_handler_;
};

// Tests that the activity can be performed when the data object shows that the
// page is printable.
TEST_F(PrintActivityTest, DataTrue_ActivityEnabled) {
  ShareToData* data = CreateData(true);
  PrintActivity* activity =
      [[PrintActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the data object shows that
// the page is not printable.
TEST_F(PrintActivityTest, DataFalse_ActivityDisabled) {
  ShareToData* data = CreateData(false);
  PrintActivity* activity =
      [[PrintActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity triggers the right handler method.
TEST_F(PrintActivityTest, ExecuteActivity_CallsHandler) {
  [[mocked_handler_ expect] printTab];

  ShareToData* data = CreateData(true);
  PrintActivity* activity =
      [[PrintActivity alloc] initWithData:data handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
