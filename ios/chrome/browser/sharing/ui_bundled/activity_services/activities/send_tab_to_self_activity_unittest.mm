// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"

#import "components/send_tab_to_self/metrics_util.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "testing/gtest_mac.h"
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

    mocked_handler_ = OCMStrictProtocolMock(@protocol(SendTabToSelfCommands));
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

  [[mocked_handler_ expect]
      showSendTabToSelfUI:data.shareURL
                    title:data.title
               entryPoint:send_tab_to_self::ShareEntryPoint::kShareSheet];

  SendTabToSelfActivity* activity =
      [[SendTabToSelfActivity alloc] initWithData:data handler:mocked_handler_];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}

// Tests that a device-specific activity returns the correct, dynamically
// formatted title containing the device name.
TEST_F(SendTabToSelfActivityTest, DeviceSpecific_ActivityTitle) {
  ShareToData* data = CreateData(true);
  NSString* activity_title = @"Tormund • My MacBook Pro";
  SendTabToSelfShareActivity* activity = [[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:activity_title
          cacheGUID:@"guid_mac"
         deviceName:@"My MacBook Pro"
         formFactor:syncer::DeviceInfo::FormFactor::kDesktop];

  EXPECT_NSEQ(activity_title, [activity activityTitle]);
}

// Tests that device-specific activities return valid images for each form
// factor.
TEST_F(SendTabToSelfActivityTest, DeviceSpecific_ActivityImage) {
  ShareToData* data = CreateData(true);

  SendTabToSelfShareActivity* phone_activity =
      [[SendTabToSelfShareActivity alloc]
           initWithData:data
                handler:mocked_handler_
          activityTitle:@"Phone"
              cacheGUID:@"guid1"
             deviceName:@"Phone"
             formFactor:syncer::DeviceInfo::FormFactor::kPhone];
  EXPECT_NE(nil, [phone_activity activityImage]);

  SendTabToSelfShareActivity* tablet_activity =
      [[SendTabToSelfShareActivity alloc]
           initWithData:data
                handler:mocked_handler_
          activityTitle:@"Tablet"
              cacheGUID:@"guid2"
             deviceName:@"Tablet"
             formFactor:syncer::DeviceInfo::FormFactor::kTablet];
  EXPECT_NE(nil, [tablet_activity activityImage]);

  SendTabToSelfShareActivity* desktop_activity =
      [[SendTabToSelfShareActivity alloc]
           initWithData:data
                handler:mocked_handler_
          activityTitle:@"Desktop"
              cacheGUID:@"guid3"
             deviceName:@"Desktop"
             formFactor:syncer::DeviceInfo::FormFactor::kDesktop];
  EXPECT_NE(nil, [desktop_activity activityImage]);
}

// Tests that executing a device-specific activity triggers the direct-send
// coordinator command.
TEST_F(SendTabToSelfActivityTest,
       ExecuteDeviceSpecificActivity_CallsDirectSendHandler) {
  ShareToData* data = CreateData(true);
  NSString* activity_title = @"Tormund • My Pixel 8";
  NSString* cache_guid = @"pixel_8_guid";

  [[mocked_handler_ expect]
      sendTabToSelfToDeviceWithURL:data.shareURL
                             title:data.title
                          deviceID:cache_guid
                        deviceName:@"My Pixel 8"
                        entryPoint:send_tab_to_self::ShareEntryPoint::
                                       kShareSheetDirectShare];

  SendTabToSelfShareActivity* activity = [[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:activity_title
          cacheGUID:cache_guid
         deviceName:@"My Pixel 8"
         formFactor:syncer::DeviceInfo::FormFactor::kPhone];

  id activity_partial_mock = OCMPartialMock(activity);
  [[activity_partial_mock expect] activityDidFinish:YES];

  [activity performActivity];

  [mocked_handler_ verify];
  [activity_partial_mock verify];
}

// Tests that the generic and device-specific activities return their correct,
// respective categories.
TEST_F(SendTabToSelfActivityTest, ActivityCategory) {
  EXPECT_EQ(UIActivityCategoryAction, [SendTabToSelfActivity activityCategory]);
  EXPECT_EQ(UIActivityCategoryShare,
            [SendTabToSelfShareActivity activityCategory]);
}

// Tests that `sendTabToSelfActivitiesForData` returns an empty array when the
// URL is invalid.
TEST_F(SendTabToSelfActivityTest,
       SendTabToSelfActivitiesForData_InvalidURLReturnsEmpty) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("chrome://version")
                                 visibleURL:GURL("chrome://version")
                                      title:@"Version"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:nil
                               linkMetadata:nil];

  NSArray<UIActivity*>* activities =
      [SendTabToSelfActivity sendTabToSelfActivitiesForData:data
                                                syncService:nullptr
                                                    handler:mocked_handler_
                                              userGivenName:@"John"];
  EXPECT_EQ(0u, activities.count);
}

// Tests that `sendTabToSelfActivitiesForData` returns the generic activity when
// the sync service is null.
TEST_F(SendTabToSelfActivityTest,
       SendTabToSelfActivitiesForData_NullSyncServiceReturnsGenericActivity) {
  ShareToData* data = CreateData(true);
  NSArray<UIActivity*>* activities =
      [SendTabToSelfActivity sendTabToSelfActivitiesForData:data
                                                syncService:nullptr
                                                    handler:mocked_handler_
                                              userGivenName:@"John"];
  ASSERT_EQ(1u, activities.count);
  EXPECT_TRUE(
      [activities.firstObject isKindOfClass:[SendTabToSelfActivity class]]);
}
