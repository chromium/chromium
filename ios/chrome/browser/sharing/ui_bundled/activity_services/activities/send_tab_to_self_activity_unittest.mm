// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"

#import "base/test/gtest_util.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

NSString* const kTestMacGuid = @"9c20a8d6-4e5a-4b92-801b-c1285dbb1a8d";
NSString* const kTestPhoneGuid = @"e2b3c4d5-6f7a-4b8c-9d0e-1f2a3b4c5d6e";
NSString* const kTestTabletGuid = @"b8451b6e-41d1-419b-a320-22c67420e7df";
NSString* const kTestDesktopGuid = @"a1b2c3d4-e5f6-4a1b-8c2d-3e4f5a6b7c8d";
NSString* const kTestPixel8Guid = @"d4e5f6a7-b8c9-4d0e-1f2a-3b4c5d6e7f8a";

}  // namespace

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
                                       thumbnail:nil
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
          cacheGUID:kTestMacGuid
         deviceName:@"My MacBook Pro"
         formFactor:syncer::DeviceInfo::FormFactor::kDesktop
             osType:syncer::DeviceInfo::OsType::kMac];

  EXPECT_NSEQ(activity_title, [activity activityTitle]);
}

// Tests that a device-specific activity returns a unique activity type keyed by
// the target device's cache GUID.
TEST_F(SendTabToSelfActivityTest,
       DeviceSpecific_ActivityTypeReturnsGuidSuffixedIdentifier) {
  ShareToData* data = CreateData(true);
  SendTabToSelfShareActivity* activity = [[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:@"My MacBook Pro"
          cacheGUID:kTestMacGuid
         deviceName:@"My MacBook Pro"
         formFactor:syncer::DeviceInfo::FormFactor::kDesktop
             osType:syncer::DeviceInfo::OsType::kMac];

  NSString* expected_activity_type =
      [NSString stringWithFormat:@"com.google.chrome.sendTabToSelfActivity.%@",
                                 kTestMacGuid];
  EXPECT_NSEQ(expected_activity_type, [activity activityType]);
}

// Tests that initializing a device-specific activity with an empty or nil cache
// GUID triggers a CHECK failure.
TEST_F(SendTabToSelfActivityTest,
       DeviceSpecific_InitWithEmptyOrNilGuidCrashes) {
  ShareToData* data = CreateData(true);

  // Passing an empty string violates the cache GUID non-empty precondition.
  EXPECT_CHECK_DEATH((void)[[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:@"My MacBook Pro"
          cacheGUID:@""
         deviceName:@"My MacBook Pro"
         formFactor:syncer::DeviceInfo::FormFactor::kDesktop
             osType:syncer::DeviceInfo::OsType::kMac]);

  // Passing nil violates the cache GUID non-empty precondition.
  EXPECT_CHECK_DEATH((void)[[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:@"My MacBook Pro"
          cacheGUID:nil
         deviceName:@"My MacBook Pro"
         formFactor:syncer::DeviceInfo::FormFactor::kDesktop
             osType:syncer::DeviceInfo::OsType::kMac]);
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
              cacheGUID:kTestPhoneGuid
             deviceName:@"Phone"
             formFactor:syncer::DeviceInfo::FormFactor::kPhone
                 osType:syncer::DeviceInfo::OsType::kIOS];
  EXPECT_NE(nil, [phone_activity activityImage]);

  SendTabToSelfShareActivity* tablet_activity =
      [[SendTabToSelfShareActivity alloc]
           initWithData:data
                handler:mocked_handler_
          activityTitle:@"Tablet"
              cacheGUID:kTestTabletGuid
             deviceName:@"Tablet"
             formFactor:syncer::DeviceInfo::FormFactor::kTablet
                 osType:syncer::DeviceInfo::OsType::kIOS];
  EXPECT_NE(nil, [tablet_activity activityImage]);

  SendTabToSelfShareActivity* desktop_activity =
      [[SendTabToSelfShareActivity alloc]
           initWithData:data
                handler:mocked_handler_
          activityTitle:@"Desktop"
              cacheGUID:kTestDesktopGuid
             deviceName:@"Desktop"
             formFactor:syncer::DeviceInfo::FormFactor::kDesktop
                 osType:syncer::DeviceInfo::OsType::kMac];
  EXPECT_NE(nil, [desktop_activity activityImage]);
}

// Tests that executing a device-specific activity triggers the direct-send
// coordinator command.
TEST_F(SendTabToSelfActivityTest,
       ExecuteDeviceSpecificActivity_CallsDirectSendHandler) {
  ShareToData* data = CreateData(true);
  NSString* activity_title = @"Tormund • My Pixel 8";

  [[mocked_handler_ expect]
      sendTabToSelfToDeviceWithURL:data.shareURL
                             title:data.title
                          deviceID:kTestPixel8Guid
                        deviceName:@"My Pixel 8"
                        entryPoint:send_tab_to_self::ShareEntryPoint::
                                       kShareSheetDirectShare];

  SendTabToSelfShareActivity* activity = [[SendTabToSelfShareActivity alloc]
       initWithData:data
            handler:mocked_handler_
      activityTitle:activity_title
          cacheGUID:kTestPixel8Guid
         deviceName:@"My Pixel 8"
         formFactor:syncer::DeviceInfo::FormFactor::kPhone
             osType:syncer::DeviceInfo::OsType::kAndroid];

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
                                  thumbnail:nil
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
