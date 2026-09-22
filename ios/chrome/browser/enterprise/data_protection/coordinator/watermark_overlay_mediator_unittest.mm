// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/watermark_overlay_mediator.h"

#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/model/watermark_request_config.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_consumer.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// A fake consumer implementing WatermarkConsumer to verify state changes.
@interface FakeWatermarkConsumer : NSObject <WatermarkConsumer>
@property(nonatomic, copy) NSString* text;
@property(nonatomic, assign) WatermarkStyle style;
@property(nonatomic, assign) NSInteger updateCount;
@end

@implementation FakeWatermarkConsumer

- (void)updateWatermarkWithText:(NSString*)text style:(WatermarkStyle)style {
  _text = [text copy];
  _style = style;
  _updateCount++;
}

@end

// Test fixture for `WatermarkOverlayMediator`.
class WatermarkOverlayMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    enterprise_connectors::RegisterProfilePrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple pref_service_;
};

// Tests that the mediator correctly propagates standard valid preference values
// to the consumer.
TEST_F(WatermarkOverlayMediatorTest, StandardPreferences) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, 35);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, 15);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           36);

  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<WatermarkRequestConfig>("Top Secret");
  WatermarkOverlayMediator* mediator =
      [[WatermarkOverlayMediator alloc] initWithRequest:request.get()
                                            prefService:&pref_service_];
  FakeWatermarkConsumer* consumer = [[FakeWatermarkConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.updateCount, 1);
  EXPECT_NSEQ(@"Top Secret", consumer.text);
  EXPECT_TRUE(consumer.style.fill_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.fill_opacity, 0.35f);
  EXPECT_TRUE(consumer.style.outline_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.outline_opacity, 0.15f);
  EXPECT_TRUE(consumer.style.font_size.has_value());
  EXPECT_EQ(*consumer.style.font_size, 36);
}

// Tests that the mediator clamps out-of-range low preference values correctly.
TEST_F(WatermarkOverlayMediatorTest, ClampingLowValues) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, -10);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, -5);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           0);

  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<WatermarkRequestConfig>("Clamped Low");
  WatermarkOverlayMediator* mediator =
      [[WatermarkOverlayMediator alloc] initWithRequest:request.get()
                                            prefService:&pref_service_];
  FakeWatermarkConsumer* consumer = [[FakeWatermarkConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.updateCount, 1);
  EXPECT_NSEQ(@"Clamped Low", consumer.text);
  EXPECT_TRUE(consumer.style.fill_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.fill_opacity, 0.0f);
  EXPECT_TRUE(consumer.style.outline_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.outline_opacity, 0.0f);
  EXPECT_TRUE(consumer.style.font_size.has_value());
  EXPECT_EQ(*consumer.style.font_size, 1);
}

// Tests that the mediator clamps out-of-range high preference values correctly.
TEST_F(WatermarkOverlayMediatorTest, ClampingHighValues) {
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleFillOpacityPref, 150);
  pref_service_.SetInteger(
      enterprise_connectors::kWatermarkStyleOutlineOpacityPref, 200);
  pref_service_.SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref,
                           600);

  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<WatermarkRequestConfig>("Clamped High");
  WatermarkOverlayMediator* mediator =
      [[WatermarkOverlayMediator alloc] initWithRequest:request.get()
                                            prefService:&pref_service_];
  FakeWatermarkConsumer* consumer = [[FakeWatermarkConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.updateCount, 1);
  EXPECT_NSEQ(@"Clamped High", consumer.text);
  EXPECT_TRUE(consumer.style.fill_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.fill_opacity, 1.0f);
  EXPECT_TRUE(consumer.style.outline_opacity.has_value());
  EXPECT_FLOAT_EQ(*consumer.style.outline_opacity, 1.0f);
  EXPECT_TRUE(consumer.style.font_size.has_value());
  EXPECT_EQ(*consumer.style.font_size, 500);
}

// Tests that when PrefService is null, the style's optionals are empty.
TEST_F(WatermarkOverlayMediatorTest, NullPrefService) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<WatermarkRequestConfig>("No Prefs");
  WatermarkOverlayMediator* mediator =
      [[WatermarkOverlayMediator alloc] initWithRequest:request.get()
                                            prefService:nullptr];
  FakeWatermarkConsumer* consumer = [[FakeWatermarkConsumer alloc] init];
  mediator.consumer = consumer;

  EXPECT_EQ(consumer.updateCount, 1);
  EXPECT_NSEQ(@"No Prefs", consumer.text);
  EXPECT_FALSE(consumer.style.fill_opacity.has_value());
  EXPECT_FALSE(consumer.style.outline_opacity.has_value());
  EXPECT_FALSE(consumer.style.font_size.has_value());
}

// Tests that disconnecting the mediator clears the consumer reference.
TEST_F(WatermarkOverlayMediatorTest, Disconnect) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<WatermarkRequestConfig>(
          "Disconnect Test");
  WatermarkOverlayMediator* mediator =
      [[WatermarkOverlayMediator alloc] initWithRequest:request.get()
                                            prefService:&pref_service_];
  FakeWatermarkConsumer* consumer = [[FakeWatermarkConsumer alloc] init];
  mediator.consumer = consumer;
  EXPECT_EQ(consumer.updateCount, 1);

  [mediator disconnect];
  EXPECT_EQ(mediator.consumer, nil);
}
