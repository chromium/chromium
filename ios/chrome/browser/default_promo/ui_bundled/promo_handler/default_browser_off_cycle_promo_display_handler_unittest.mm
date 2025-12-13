// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/promo_handler/default_browser_off_cycle_promo_display_handler.h"

#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakePromosManagerCommandHandlerForOffCyclePromo
    : NSObject <PromosManagerCommands>

@property(nonatomic, assign) BOOL showDefaultBrowserPromoCalled;

@end

@implementation FakePromosManagerCommandHandlerForOffCyclePromo

- (void)showPromo {
}

- (void)showAppStoreReviewPrompt {
}

- (void)showSignin:(ShowSigninCommand*)command {
}

- (void)showWhatsNewPromo {
}

- (void)showChoicePromo {
}

- (void)showDefaultBrowserPromo {
  self.showDefaultBrowserPromoCalled = YES;
}

- (void)showDefaultBrowserPromoAfterRemindMeLater {
}

- (void)showOmniboxPositionChoicePromo {
}

- (void)showFullscreenSigninPromo {
}

- (void)showWelcomeBackPromo {
}

@end

class DefaultBrowserOffCyclePromoDisplayHandlerTest : public PlatformTest {
 public:
  DefaultBrowserOffCyclePromoDisplayHandlerTest() : PlatformTest() {}
};

TEST_F(DefaultBrowserOffCyclePromoDisplayHandlerTest, TestConfig) {
  DefaultBrowserOffCyclePromoDisplayHandler* handler =
      [[DefaultBrowserOffCyclePromoDisplayHandler alloc] init];
  PromoConfig config = [handler config];
  EXPECT_EQ(promos_manager::Promo::DefaultBrowserOffCycle, config.identifier);
  EXPECT_EQ(&feature_engagement::kIPHiOSDefaultBrowserOffCyclePromoFeature,
            config.feature_engagement_feature);
}

TEST_F(DefaultBrowserOffCyclePromoDisplayHandlerTest, TestHandleDisplay) {
  DefaultBrowserOffCyclePromoDisplayHandler* display_handler =
      [[DefaultBrowserOffCyclePromoDisplayHandler alloc] init];
  FakePromosManagerCommandHandlerForOffCyclePromo* command_handler =
      [[FakePromosManagerCommandHandlerForOffCyclePromo alloc] init];
  display_handler.handler = command_handler;

  // Calls method and checks that the fake display handler has been notified.
  [display_handler handleDisplay];
  EXPECT_TRUE(command_handler.showDefaultBrowserPromoCalled);
}
