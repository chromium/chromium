// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_mediator.h"

#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_generation_commands.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/generate_qr_code_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_image_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_text_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_url_source.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/ui/sharing/sharing_scenario.h"
#import "ios/web/common/user_agent.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol HandlerProtocols <FindInPageCommands>
@end

class ActivityServiceMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    mocked_handler_ = OCMStrictProtocolMock(@protocol(HandlerProtocols));
    mocked_bookmarks_handler_ =
        OCMStrictProtocolMock(@protocol(BookmarksCommands));
    mocked_help_handler_ = OCMStrictProtocolMock(@protocol(HelpCommands));
    mocked_qr_generation_handler_ =
        OCMStrictProtocolMock(@protocol(QRGenerationCommands));
    mocked_thumbnail_generator_ =
        OCMStrictClassMock([ChromeActivityItemThumbnailGenerator class]);

    mediator_ = [[ActivityServiceMediator alloc]
                initWithHandler:mocked_handler_
               bookmarksHandler:mocked_bookmarks_handler_
                    helpHandler:mocked_help_handler_
            qrGenerationHandler:mocked_qr_generation_handler_
                    prefService:pref_service_.get()
                  bookmarkModel:nil
             baseViewController:nil
                navigationAgent:nil
        readingListBrowserAgent:nil];

    pref_service_->registry()->RegisterBooleanPref(prefs::kPrintingEnabled,
                                                   true);
    pref_service_->registry()->RegisterIntegerPref(prefs::kIosShareChromeCount,
                                                   0);
    pref_service_->registry()->RegisterTimePref(prefs::kIosShareChromeLastShare,
                                                base::Time());
  }

  void VerifyTypes(NSArray* activities, NSArray* expected_types) {
    EXPECT_EQ([expected_types count], [activities count]);
    for (unsigned int i = 0U; i < [activities count]; i++) {
      EXPECT_TRUE([activities[i] isKindOfClass:expected_types[i]]);
    }
  }

  id mocked_handler_;
  id mocked_bookmarks_handler_;
  id mocked_help_handler_;
  id mocked_qr_generation_handler_;
  id mocked_thumbnail_generator_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::HistogramTester histograms_tester_;

  ActivityServiceMediator* mediator_;
};

// Tests that only one ChromeActivityURLSource is initialized from a ShareToData
// instance without additional text.
TEST_F(ActivityServiceMediatorTest, ActivityItemsForMulitpleDataItems_Success) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                 visibleURL:GURL("https://google.com/")
                                      title:@"Some Title"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray<id<ChromeActivityItemSource>>* activityItems =
      [mediator_ activityItemsForDataItems:@[ data ]];

  EXPECT_EQ(1U, [activityItems count]);
  EXPECT_TRUE([activityItems[0] isKindOfClass:[ChromeActivityURLSource class]]);
}

// Tests that two activity items are created from a ShareToData instance with
// additional text.
TEST_F(ActivityServiceMediatorTest,
       ActivityItemsForData_WithAdditionalText_Success) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                 visibleURL:GURL("https://google.com/")
                                      title:@"Some Title"
                             additionalText:@"Foo, bar!"
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray<id<ChromeActivityItemSource>>* activityItems =
      [mediator_ activityItemsForDataItems:@[ data ]];

  EXPECT_EQ(2U, [activityItems count]);
  EXPECT_TRUE(
      [activityItems[0] isKindOfClass:[ChromeActivityTextSource class]]);
  EXPECT_TRUE([activityItems[1] isKindOfClass:[ChromeActivityURLSource class]]);
}

// Tests that separate ChromeActivityURLSource instances are initialized for
// each ShareToData instance.
TEST_F(ActivityServiceMediatorTest,
       ActivityItemsForData_NoAdditionalText_Success) {
  ShareToData* data1 =
      [[ShareToData alloc] initWithShareURL:GURL("https://www.google.com/")
                                 visibleURL:GURL("https://google.com/")
                                      title:@"Some Title"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  ShareToData* data2 =
      [[ShareToData alloc] initWithShareURL:GURL("https://www.example.com/")
                                 visibleURL:GURL("https://example.com/")
                                      title:@"Another Title"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray<id<ChromeActivityItemSource>>* activityItems =
      [mediator_ activityItemsForDataItems:@[ data1, data2 ]];

  EXPECT_EQ(2U, [activityItems count]);
  EXPECT_TRUE([activityItems[0] isKindOfClass:[ChromeActivityURLSource class]]);
  EXPECT_TRUE([activityItems[1] isKindOfClass:[ChromeActivityURLSource class]]);
}

// Tests that only the CopyActivity and PrintActivity get added for a page that
// is not HTTP or HTTPS.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_NotHTTPOrHTTPS) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("chrome://chromium.org/")
                                 visibleURL:GURL("chrome://chromium.org/")
                                      title:@"baz"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray* activities = [mediator_ applicationActivitiesForDataItems:@[ data ]];

  // Verify activities' types.
  VerifyTypes(activities, @[ [CopyActivity class], [PrintActivity class] ]);
}

// Tests that the right activities are added in order for an HTTP page.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_HTTP) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("http://example.com")
                                 visibleURL:GURL("http://example.com")
                                      title:@"baz"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray* activities = [mediator_ applicationActivitiesForDataItems:@[ data ]];

  // Verify activities' types.
  VerifyTypes(activities, @[
    [CopyActivity class], [SendTabToSelfActivity class],
    [ReadingListActivity class], [BookmarkActivity class],
    [GenerateQrCodeActivity class], [FindInPageActivity class],
    [RequestDesktopOrMobileSiteActivity class], [PrintActivity class]
  ]);
}

// Tests that the right activities are added in order for an HTTPS page.
TEST_F(ActivityServiceMediatorTest, ActivitiesForData_HTTPS) {
  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("https://example.com")
                                 visibleURL:GURL("https://example.com")
                                      title:@"baz"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray* activities = [mediator_ applicationActivitiesForDataItems:@[ data ]];

  // Verify activities' types.
  VerifyTypes(activities, @[
    [CopyActivity class], [SendTabToSelfActivity class],
    [ReadingListActivity class], [BookmarkActivity class],
    [GenerateQrCodeActivity class], [FindInPageActivity class],
    [RequestDesktopOrMobileSiteActivity class], [PrintActivity class]
  ]);
}

// Tests that only the CopyActivity is available for multiple data items.
TEST_F(ActivityServiceMediatorTest, ActivitiesForMultipleDataItems) {
  ShareToData* data1 =
      [[ShareToData alloc] initWithShareURL:GURL("https://google.com")
                                 visibleURL:GURL("https://google.com")
                                      title:@"Title"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];
  ShareToData* data2 =
      [[ShareToData alloc] initWithShareURL:GURL("https://example.com")
                                 visibleURL:GURL("https://example.com")
                                      title:@"baz"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray* activities =
      [mediator_ applicationActivitiesForDataItems:@[ data1, data2 ]];

  VerifyTypes(activities, @[ [CopyActivity class] ]);
}

// Tests that only one ChromeActivityImageSource is initialized from a
// ShareIamgeData instance.
TEST_F(ActivityServiceMediatorTest, ActivityItemsForImageData_Success) {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:[[UIImage alloc] init]
                                      title:@"some title"];

  NSArray<ChromeActivityImageSource*>* activityItems =
      [mediator_ activityItemsForImageData:data];

  EXPECT_EQ(1U, [activityItems count]);
}

// Tests that the right activities are added in order for an image.
TEST_F(ActivityServiceMediatorTest, ActivitiesForImageData) {
  ShareImageData* data =
      [[ShareImageData alloc] initWithImage:[[UIImage alloc] init]
                                      title:@"some title"];

  NSArray* activities = [mediator_ applicationActivitiesForImageData:data];

  // For now, we only customize the print activity.
  EXPECT_EQ(1U, [activities count]);
  VerifyTypes(activities, @[ [PrintActivity class] ]);
}

// Tests that computing the list of excluded activities works for one URL.
TEST_F(ActivityServiceMediatorTest, ExcludedActivityTypes_SingleItemURL) {
  ChromeActivityURLSource* activityURLSource = [[ChromeActivityURLSource alloc]
      initWithShareURL:[NSURL URLWithString:@"https://example.com"]
               subject:@"Does not matter"];

  NSSet* expectedSet = [NSSet setWithArray:@[
    UIActivityTypeAddToReadingList, UIActivityTypeCopyToPasteboard,
    UIActivityTypePrint, UIActivityTypeSaveToCameraRoll
  ]];
  NSSet* mediatorSet =
      [mediator_ excludedActivityTypesForItems:@[ activityURLSource ]];

  EXPECT_NSEQ(expectedSet, activityURLSource.excludedActivityTypes);
  EXPECT_NSEQ(expectedSet, mediatorSet);
}

// Tests that computing the list of excluded activities works for one image.
TEST_F(ActivityServiceMediatorTest, ExcludedActivityTypes_SingleItemImage) {
  ChromeActivityImageSource* activityImageSource =
      [[ChromeActivityImageSource alloc] initWithImage:[[UIImage alloc] init]
                                                 title:@"something"];
  NSSet* expectedSet = [NSSet setWithArray:@[
    UIActivityTypeAssignToContact,
    UIActivityTypePrint,
  ]];

  NSSet* mediatorSet =
      [mediator_ excludedActivityTypesForItems:@[ activityImageSource ]];

  EXPECT_NSEQ(expectedSet, activityImageSource.excludedActivityTypes);
  EXPECT_NSEQ(expectedSet, mediatorSet);
}

// Tests that computing the list of excluded activities works for two item.
TEST_F(ActivityServiceMediatorTest, ExcludedActivityTypes_TwoItems) {
  ChromeActivityURLSource* activityURLSource = [[ChromeActivityURLSource alloc]
      initWithShareURL:[NSURL URLWithString:@"https://example.com"]
               subject:@"Does not matter"];
  ChromeActivityImageSource* activityImageSource =
      [[ChromeActivityImageSource alloc] initWithImage:[[UIImage alloc] init]
                                                 title:@"something"];

  NSMutableSet* expectedSet = [[NSMutableSet alloc]
      initWithSet:activityURLSource.excludedActivityTypes];
  [expectedSet unionSet:activityImageSource.excludedActivityTypes];

  NSSet* computedExclusion = [mediator_ excludedActivityTypesForItems:@[
    activityURLSource, activityImageSource
  ]];

  EXPECT_TRUE([expectedSet isEqualToSet:computedExclusion]);
}

// Tests that successful action completion is wired to log a histogram.
TEST_F(ActivityServiceMediatorTest, ShareFinished_Success) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail.
  NSString* copyActivityString = @"com.google.chrome.copyActivity";
  [mediator_ shareFinishedWithScenario:SharingScenario::TabShareButton
                          activityType:copyActivityString
                             completed:YES];

  // Verify histogram is logged. Values are hardcoded as they are encapsulated
  // away.
  const char histogramName[] = "Mobile.Share.TabShareButton.Actions";
  int copyAction = 3;
  histograms_tester_.ExpectBucketCount(histogramName, copyAction, 1);
}

// Tests that successful action completion in ShareChrome scenario stores prefs
TEST_F(ActivityServiceMediatorTest, ShareFinished_SuccessShareChrome) {
  int initialCount = pref_service_->GetInteger(prefs::kIosShareChromeCount);
  base::Time startTime = base::Time::Now();
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail.
  NSString* copyActivityString = @"com.google.chrome.copyActivity";
  [mediator_ shareFinishedWithScenario:SharingScenario::ShareChrome
                          activityType:copyActivityString
                             completed:YES];
  int count = pref_service_->GetInteger(prefs::kIosShareChromeCount);
  base::Time lastShare =
      pref_service_->GetTime(prefs::kIosShareChromeLastShare);
  EXPECT_EQ(count, initialCount + 1);
  EXPECT_GT(lastShare, startTime);
}

TEST_F(ActivityServiceMediatorTest, ShareFinished_Cancel) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail. That is our success condition.
  NSString* copyActivityString = @"com.google.chrome.copyActivity";
  [mediator_ shareFinishedWithScenario:SharingScenario::TabShareButton
                          activityType:copyActivityString
                             completed:NO];

  // Verify histogram is logged. Values are hardcoded as they are encapsulated
  // away.
  const char histogramName[] = "Mobile.Share.TabShareButton.Actions";
  int cancelAction = 1;
  histograms_tester_.ExpectBucketCount(histogramName, cancelAction, 1);
}

TEST_F(ActivityServiceMediatorTest, ShareCancelled) {
  // Since mocked_handler_ is a strict mock, any call to its methods would make
  // the test fail. That is our success condition.
  [mediator_ shareFinishedWithScenario:SharingScenario::TabShareButton
                          activityType:nil
                             completed:NO];

  // Verify histogram is logged. Values are hardcoded as they are encapsulated
  // away.
  const char histogramName[] = "Mobile.Share.TabShareButton.Actions";
  int cancelAction = 1;
  histograms_tester_.ExpectBucketCount(histogramName, cancelAction, 1);
}

TEST_F(ActivityServiceMediatorTest, PrintPrefDisabled) {
  pref_service_->SetUserPref(prefs::kPrintingEnabled,
                             std::make_unique<base::Value>(false));

  ShareToData* data =
      [[ShareToData alloc] initWithShareURL:GURL("http://example.com")
                                 visibleURL:GURL("http://example.com")
                                      title:@"baz"
                             additionalText:nil
                            isOriginalTitle:YES
                            isPagePrintable:YES
                           isPageSearchable:YES
                           canSendTabToSelf:YES
                                  userAgent:web::UserAgentType::MOBILE
                         thumbnailGenerator:mocked_thumbnail_generator_
                               linkMetadata:nil];

  NSArray* activities = [mediator_ applicationActivitiesForDataItems:@[ data ]];

  // Verify activities' types.
  VerifyTypes(activities, @[
    [CopyActivity class], [SendTabToSelfActivity class],
    [ReadingListActivity class], [BookmarkActivity class],
    [GenerateQrCodeActivity class], [FindInPageActivity class],
    [RequestDesktopOrMobileSiteActivity class]
  ]);

  // Verify activities' size.
  EXPECT_EQ(7U, [activities count]);
}
