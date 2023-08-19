// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/generate_qr_code_activity.h"

#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

// Test fixture for covering the GenerateQrCodeActivity class.
class GenerateQrCodeActivityTest : public PlatformTest {
 protected:
  GenerateQrCodeActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(QRGenerationCommands));
  }

  // Creates a GenerateQrCodeActivity instance.
  GenerateQrCodeActivity* CreateActivity() {
    return
        [[GenerateQrCodeActivity alloc] initWithURL:GURL("https://example.com")
                                              title:@"Some title"
                                            handler:mocked_handler_];
  }

  id mocked_handler_;
};

// Tests that the activity can be performed when the handler is not nil.
TEST_F(GenerateQrCodeActivityTest, ValidHandler_ActivityEnabled) {
  GenerateQrCodeActivity* activity = CreateActivity();

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the activity cannot be performed when its handler is nil.
TEST_F(GenerateQrCodeActivityTest, NilHandler_ActivityDisabled) {
  GenerateQrCodeActivity* activity =
      [[GenerateQrCodeActivity alloc] initWithURL:GURL("https://example.com")
                                            title:@"Some title"
                                          handler:nil];

  EXPECT_FALSE([activity canPerformWithActivityItems:@[]]);
}

// Tests that the right handler method is invoked upon execution.
TEST_F(GenerateQrCodeActivityTest, Execution) {
  __block GURL fakeURL("https://example.com");
  __block NSString* fakeTitle = @"fake title";

  [[mocked_handler_ expect]
      generateQRCode:[OCMArg
                         checkWithBlock:^BOOL(GenerateQRCodeCommand* value) {
                           EXPECT_EQ(fakeURL, value.URL);
                           EXPECT_EQ(fakeTitle, value.title);
                           return YES;
                         }]];

  GenerateQrCodeActivity* activity =
      [[GenerateQrCodeActivity alloc] initWithURL:fakeURL
                                            title:fakeTitle
                                          handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}
