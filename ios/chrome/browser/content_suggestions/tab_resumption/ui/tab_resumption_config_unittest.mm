// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_config.h"

#import "base/time/time.h"
#import "components/segmentation_platform/public/trigger.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

@interface FakeCommandHandler : NSObject <TabResumptionCommands>
@end

@implementation FakeCommandHandler

- (void)openTabResumptionItem:(TabResumptionConfig*)config {
}

- (void)trackShopCardItem:(TabResumptionConfig*)config {
}

@end

using TabResumptionConfigTest = PlatformTest;

// Tests that item is correctly reconfigured.
TEST_F(TabResumptionConfigTest, ReconfigureItem) {
  web::FakeWebState fake_web_state;
  FakeCommandHandler* command_handler = [[FakeCommandHandler alloc] init];
  TabResumptionConfig* config = [[TabResumptionConfig alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  config.sessionName = @"session a";
  config.tabTitle = @"title a";
  config.tabURL = GURL("https://a");
  config.reason = @"You visited this web site 1 hour ago";
  config.syncedTime = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(1));
  config.faviconImage = DefaultSettingsRootSymbol(@"circle");
  config.contentImage = DefaultSettingsRootSymbol(@"flame");
  config.commandHandler = command_handler;
  config.URLKey = std::string("url key a");
  config.requestID =
      segmentation_platform::TrainingRequestId::FromUnsafeValue(1);

  TabResumptionConfig* config2 = [[TabResumptionConfig alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  config2.sessionName = @"session b";
  config2.localWebState = fake_web_state.GetWeakPtr();
  config2.tabTitle = @"title b";
  config2.tabURL = GURL("https://b");
  config2.reason = @"You may also like this web site";
  config2.syncedTime = base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(2));
  config2.faviconImage = DefaultSettingsRootSymbol(@"link");
  config2.contentImage = DefaultSettingsRootSymbol(@"trash");
  config2.commandHandler = command_handler;
  config2.URLKey = std::string("url key b");
  config2.requestID =
      segmentation_platform::TrainingRequestId::FromUnsafeValue(2);
  config2.shopCardData = [[ShopCardData alloc] init];
  config2.shopCardData.priceDrop = std::make_optional<PriceDrop>();
  config2.shopCardData.shopCardItemType = ShopCardItemType::kPriceDropOnTab;
  config2.shopCardData.priceDrop->current_price = @"$2.87";
  config2.shopCardData.priceDrop->previous_price = @"$3.14";

  [config reconfigureWithConfig:config2];
  EXPECT_EQ(config.itemType, TabResumptionItemType::kMostRecentTab);
  EXPECT_EQ(config.localWebState.get(), &fake_web_state);
  EXPECT_NSEQ(config.sessionName, @"session b");
  EXPECT_NSEQ(config.tabTitle, @"title b");
  EXPECT_NSEQ(config.reason, @"You may also like this web site");
  EXPECT_EQ(config.tabURL, GURL("https://b"));
  EXPECT_EQ(config.syncedTime,
            base::Time::FromDeltaSinceWindowsEpoch(base::Seconds(2)));
  EXPECT_NSEQ(config.faviconImage, config2.faviconImage);
  EXPECT_NSEQ(config.contentImage, config2.contentImage);
  EXPECT_EQ(config.URLKey, "url key b");
  EXPECT_EQ(config.requestID,
            segmentation_platform::TrainingRequestId::FromUnsafeValue(2));
  EXPECT_EQ(config2.shopCardData.shopCardItemType,
            ShopCardItemType::kPriceDropOnTab);
  EXPECT_EQ(config.shopCardData.priceDrop->current_price,
            config2.shopCardData.priceDrop->current_price);
  EXPECT_EQ(config.shopCardData.priceDrop->previous_price,
            config2.shopCardData.priceDrop->previous_price);
}
