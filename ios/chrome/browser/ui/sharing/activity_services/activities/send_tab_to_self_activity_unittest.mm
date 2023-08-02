// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/send_tab_to_self_activity.h"

#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Test fixture for covering the SendTabToSelfActivity class.
class SendTabToSelfActivityTest : public PlatformTest {
 protected:
  SendTabToSelfActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
  }

  // Creates a ShareToData instance with `can_send_tab_to_self` set.
  ShareToData* CreateData(bool can_send_tab_to_self) {
    return [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                      visibleURL:GURL("https://google.com/")
                                           title:@"Some Title"
                                  additionalText:nil
                                 isOriginalTitle:YES
                                 isPagePrintable:YES
                                isPageSearchable:YES
                                canSendTabToSelf:can_send_tab_to_self
                                       userAgent:web::UserAgentType::MOBILE
                              thumbnailGenerator:nil
                                    linkMetadata:nil];
  }

  id mocked_handler_;
};

// Tests that the activity can be performed when the data object shows the tab
// can be used for STTS.
TEST_F(SendTabToSelfActivityTest, DataTrue_ActivityEnabled) {
  ShareToData* data = CreateData(true);
  SendTabToSelfActivity* activity =
      [[SendTabToSelfActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when the data object shows the
// tab cannot be used for STTS.
TEST_F(SendTabToSelfActivityTest, DataFalse_ActivityDisabled) {
  ShareToData* data = CreateData(false);
  SendTabToSelfActivity* activity =
      [[SendTabToSelfActivity alloc] initWithData:data handler:mocked_handler_];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity triggers the right handler method.
TEST_F(SendTabToSelfActivityTest, ExecuteActivity_CallsHandler) {
  ShareToData* data = CreateData(true);

  [[mocked_handler_ expect] showSendTabToSelfUI:data.shareURL title:data.title];

  SendTabToSelfActivity* activity =
      [[SendTabToSelfActivity alloc] initWithData:data handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
