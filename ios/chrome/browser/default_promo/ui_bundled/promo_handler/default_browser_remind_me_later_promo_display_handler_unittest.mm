// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/promo_handler/default_browser_remind_me_later_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakePromosManagerCommandHandler : NSObject <PromosManagerCommands>

@property(nonatomic, assign)
    BOOL displayDefaultBrowserPromoAfterRemindMeLaterCalled;

@end

@implementation FakePromosManagerCommandHandler

- (void)maybeDisplayPromo {
}

- (void)requestAppStoreReview {
}

- (void)showSignin:(ShowSigninCommand*)command {
}

- (void)showWhatsNewPromo {
}

- (void)showChoicePromo {
}

- (void)maybeDisplayDefaultBrowserPromo {
}

- (void)displayDefaultBrowserPromoAfterRemindMeLater {
  self.displayDefaultBrowserPromoAfterRemindMeLaterCalled = YES;
}

- (void)showOmniboxPositionChoicePromo {
}

@end

class DefaultBrowserRemindMeLaterPromoDisplayHandlerTest : public PlatformTest {
 public:
  DefaultBrowserRemindMeLaterPromoDisplayHandlerTest() : PlatformTest() {}
};

// Tests that the handler's config has the correct data.
TEST_F(DefaultBrowserRemindMeLaterPromoDisplayHandlerTest, TestConfig) {
  DefaultBrowserRemindMeLaterPromoDisplayHandler* handler =
      [[DefaultBrowserRemindMeLaterPromoDisplayHandler alloc] init];
  PromoConfig config = [handler config];
  EXPECT_EQ(promos_manager::Promo::DefaultBrowserRemindMeLater,
            config.identifier);
  EXPECT_EQ(&feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature,
            config.feature_engagement_feature);
}

// Tests that calling handleDisplay correctly delegates to the correct command.
TEST_F(DefaultBrowserRemindMeLaterPromoDisplayHandlerTest, TestHandleDisplay) {
  DefaultBrowserRemindMeLaterPromoDisplayHandler* display_handler =
      [[DefaultBrowserRemindMeLaterPromoDisplayHandler alloc] init];
  FakePromosManagerCommandHandler* command_handler =
      [[FakePromosManagerCommandHandler alloc] init];
  display_handler.handler = command_handler;

  // Call method and check that the fake has been notified.
  [display_handler handleDisplay];
  EXPECT_TRUE(
      command_handler.displayDefaultBrowserPromoAfterRemindMeLaterCalled);
}
