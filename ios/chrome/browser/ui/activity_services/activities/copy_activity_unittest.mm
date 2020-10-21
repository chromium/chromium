// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestShareURL[] = "https://www.google.com/";
const char kTestVisibleURL[] = "https://google.com/";
NSString* const kTestAdditionaText = @"Foo Bar";
}  // namespace

// Test fixture for covering the CopyActivity class.
class CopyActivityTest : public PlatformTest {
 protected:
  CopyActivityTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Start with a clean pasteboard.
    ClearPasteboard();
  }

  void TearDown() override {
    PlatformTest::TearDown();

    // End with a clean pasteboard.
    ClearPasteboard();
  }

  // Creates a ShareToData instance with the given |additional_text|.
  ShareToData* CreateData(NSString* additional_text) {
    return [[ShareToData alloc] initWithShareURL:GURL(kTestShareURL)
                                      visibleURL:GURL(kTestVisibleURL)
                                           title:@"Some Title"
                                  additionalText:additional_text
                                 isOriginalTitle:YES
                                 isPagePrintable:YES
                                isPageSearchable:YES
                                canSendTabToSelf:YES
                                       userAgent:web::UserAgentType::MOBILE
                              thumbnailGenerator:nil];
  }

  NSString* GetURLString() { return base::SysUTF8ToNSString(kTestShareURL); }

  NSURL* GetExpectedURL() { return [NSURL URLWithString:GetURLString()]; }
};

// Tests that the activity can be performed.
TEST_F(CopyActivityTest, ActivityEnabled) {
  ShareToData* data = CreateData(nil);
  CopyActivity* activity = [[CopyActivity alloc] initWithData:data];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity with just a URL copies it.
TEST_F(CopyActivityTest, ExecuteActivityJustURL) {
  ShareToData* data = CreateData(nil);
  CopyActivity* activity = [[CopyActivity alloc] initWithData:data];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [activity_partial_mock verify];
  NSURL* expected_url = GetExpectedURL();
  EXPECT_TRUE([expected_url isEqual:UIPasteboard.generalPasteboard.URL]);
}

// Tests that executing the activity with a URL and additional text copies them.
TEST_F(CopyActivityTest, ExecuteActivityURLAndAdditionalText) {
  ShareToData* data = CreateData(kTestAdditionaText);
  CopyActivity* activity = [[CopyActivity alloc] initWithData:data];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [activity_partial_mock verify];

  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasURLs);
  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasStrings);

  // The first pasteboard item has both a URL and string representation of the
  // test URL.
  EXPECT_TRUE(
      [GetURLString() isEqualToString:UIPasteboard.generalPasteboard.string]);
  EXPECT_TRUE([GetExpectedURL() isEqual:UIPasteboard.generalPasteboard.URL]);

  // The second pasteboard item has the additional text stored as string.
  EXPECT_TRUE(
      [kTestAdditionaText isEqual:UIPasteboard.generalPasteboard.strings[1]]);
}
