// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"

#import "base/time/time.h"
#import "components/segmentation_platform/public/trigger.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

@interface FakeCommandHandler : NSObject <TabResumptionCommands>
@end

@implementation FakeCommandHandler

- (void)openTabResumptionItem:(TabResumptionItem*)item {
}

@end

using TabResumptionItemTest = PlatformTest;

// Tests that item is correctly reconfigured.
TEST_F(TabResumptionItemTest, ReconfigureItem) {
  FakeCommandHandler* command_handler = [[FakeCommandHandler alloc] init];
  TabResumptionItem* item = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  item.sessionName = @"session a";
  item.tabTitle = @"title a";
  item.tabURL = GURL("https://a");
  item.reason = @"You visited this web site 1 hour ago";
  item.syncedTime = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(1));
  item.faviconImage = DefaultSettingsRootSymbol(@"circle");
  item.contentImage = DefaultSettingsRootSymbol(@"flame");
  item.commandHandler = command_handler;
  item.URLKey = std::string("url key a");
  item.requestID = segmentation_platform::TrainingRequestId::FromUnsafeValue(1);

  TabResumptionItem* item2 = [[TabResumptionItem alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  item2.sessionName = @"session b";
  item2.tabTitle = @"title b";
  item2.tabURL = GURL("https://b");
  item2.reason = @"You may also like this web site";
  item2.syncedTime = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(2));
  item2.faviconImage = DefaultSettingsRootSymbol(@"link");
  item2.contentImage = DefaultSettingsRootSymbol(@"trash");
  item2.commandHandler = command_handler;
  item2.URLKey = std::string("url key b");
  item2.requestID =
      segmentation_platform::TrainingRequestId::FromUnsafeValue(2);

  [item reconfigureWithItem:item2];
  EXPECT_EQ(item.itemType, TabResumptionItemType::kMostRecentTab);
  EXPECT_NSEQ(item.sessionName, @"session b");
  EXPECT_NSEQ(item.tabTitle, @"title b");
  EXPECT_NSEQ(item.reason, @"You may also like this web site");
  EXPECT_EQ(item.tabURL, GURL("https://b"));
  EXPECT_EQ(item.syncedTime,
            base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(2)));
  EXPECT_NSEQ(item.faviconImage, item2.faviconImage);
  EXPECT_NSEQ(item.contentImage, item2.contentImage);
  EXPECT_EQ(item.URLKey, "url key b");
  EXPECT_EQ(item.requestID,
            segmentation_platform::TrainingRequestId::FromUnsafeValue(2));
}
