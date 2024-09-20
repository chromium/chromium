// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/copy_activity.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {
const char kTestShareURL[] = "https://www.google.com/";
const char kTestVisibleURL[] = "https://google.com/";
const char kSecondaryTestShareURL[] = "https://www.example.com/";
const char kSecondaryTestVisibleURL[] = "https://example.com/";
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

  // Creates a ShareToData instance with the given `additional_text`.
  ShareToData* CreateData(NSString* additional_text) {
    return CreateData(kTestShareURL, kTestVisibleURL, additional_text);
  }

  ShareToData* CreateSecondaryData() {
    return CreateData(kSecondaryTestShareURL, kSecondaryTestVisibleURL,
                      /*additional_text=*/nil);
  }

  ShareToData* CreateData(std::string share_url,
                          std::string visible_url,
                          NSString* additional_text) {
    return [[ShareToData alloc] initWithShareURL:GURL(share_url)
                                      visibleURL:GURL(visible_url)
                                           title:@"Some Title"
                                  additionalText:additional_text
                                 isOriginalTitle:YES
                                 isPagePrintable:YES
                                isPageSearchable:YES
                                canSendTabToSelf:YES
                                       userAgent:web::UserAgentType::MOBILE
                              thumbnailGenerator:nil
                                    linkMetadata:nil];
  }

  NSString* GetURLString() { return base::SysUTF8ToNSString(kTestShareURL); }

  NSURL* GetExpectedURL() { return [NSURL URLWithString:GetURLString()]; }

  NSString* GetSecondaryURLString() {
    return base::SysUTF8ToNSString(kSecondaryTestShareURL);
  }

  NSURL* GetSecondaryExpectedURL() {
    return [NSURL URLWithString:GetSecondaryURLString()];
  }
};

// Tests that the activity can be performed.
TEST_F(CopyActivityTest, ActivityEnabled) {
  ShareToData* data = CreateData(nil);
  CopyActivity* activity = [[CopyActivity alloc] initWithDataItems:@[ data ]];

  EXPECT_TRUE([activity canPerformWithActivityItems:@[]]);
}

// Tests that executing the activity with just a URL copies it.
TEST_F(CopyActivityTest, ExecuteActivityJustURL) {
  ShareToData* data = CreateData(nil);
  CopyActivity* activity = [[CopyActivity alloc] initWithDataItems:@[ data ]];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [activity_partial_mock verify];
  NSURL* expected_url = GetExpectedURL();
  EXPECT_TRUE([expected_url isEqual:UIPasteboard.generalPasteboard.URL]);
}

// Tests that executing the activity with two URLs copies them.
TEST_F(CopyActivityTest, ExecuteActivityMultipleURLs) {
  ShareToData* data = CreateData(nil);

  CopyActivity* activity =
      [[CopyActivity alloc] initWithDataItems:@[ data, CreateSecondaryData() ]];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [activity_partial_mock verify];

  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasURLs);
  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasStrings);
  EXPECT_EQ(2, UIPasteboard.generalPasteboard.numberOfItems);

  NSArray<NSURL*>* expected_urls =
      @[ GetExpectedURL(), GetSecondaryExpectedURL() ];
  EXPECT_TRUE([expected_urls isEqual:UIPasteboard.generalPasteboard.URLs]);

  NSArray<NSString*>* expected_strings =
      @[ GetURLString(), GetSecondaryURLString() ];
  EXPECT_TRUE(
      [expected_strings isEqual:UIPasteboard.generalPasteboard.strings]);
}

// Tests that executing the activity with a URL and additional text copies them.
TEST_F(CopyActivityTest, ExecuteActivityURLAndAdditionalText) {
  ShareToData* data = CreateData(kTestAdditionaText);
  CopyActivity* activity = [[CopyActivity alloc] initWithDataItems:@[ data ]];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [activity_partial_mock verify];

  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasURLs);
  ASSERT_TRUE(UIPasteboard.generalPasteboard.hasStrings);

  // The first pasteboard item has both a URL and string representation of the
  // test URL.
  EXPECT_NSEQ(GetURLString(), UIPasteboard.generalPasteboard.string);
  EXPECT_TRUE([GetExpectedURL() isEqual:UIPasteboard.generalPasteboard.URL]);

  // The second pasteboard item has the additional text stored as string.
  EXPECT_NSEQ(kTestAdditionaText, UIPasteboard.generalPasteboard.strings[1]);
}
