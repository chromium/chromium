// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The size of the symbol image.
CGFloat kSymbolImagePointSize = 18.;

// Returns the branded version of the Google shield symbol.
UIImage* GetBrandedGoogleShieldSymbol() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  return CustomSymbolWithPointSize(kGoogleShieldSymbol, kSymbolImagePointSize);
#else
  return CustomSymbolWithPointSize(kShieldSymbol, kSymbolImagePointSize);
#endif
}

}  // namespace

using safe_browsing::MockTailoredSecurityServiceInfobarDelegate;
using safe_browsing::TailoredSecurityServiceInfobarDelegate;
using safe_browsing::TailoredSecurityServiceMessageState;
using tailored_security_service_infobar_overlays::
    TailoredSecurityServiceBannerRequestConfig;

// Test fixture for TailoredSecurityInfobarBannerOverlayMediator.
using TailoredSecurityInfobarBannerOverlayMediatorTest = PlatformTest;

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly sets
// up its consumer.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  // Create an InfoBarIOS with a TailoredSecurityServiceInfobarDelegate.
  std::unique_ptr<TailoredSecurityServiceInfobarDelegate> passed_delegate =
      MockTailoredSecurityServiceInfobarDelegate::Create(
          /*message_state*/ TailoredSecurityServiceMessageState::
              kConsentedAndFlowEnabled,
          nullptr);
  TailoredSecurityServiceInfobarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeTailoredSecurityService,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      TailoredSecurityServiceBannerRequestConfig>(&infobar);
  TailoredSecurityInfobarBannerOverlayMediator* mediator =
      [[TailoredSecurityInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());
  NSString* subtitle = base::SysUTF16ToNSString(delegate->GetDescription());
  NSString* buttonText =
      base::SysUTF16ToNSString(delegate->GetMessageActionText());
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  EXPECT_NSEQ(bannerAccessibilityLabel, consumer.bannerAccessibilityLabel);
  EXPECT_NSEQ(buttonText, consumer.buttonText);
  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(subtitle, consumer.subtitleText);
  EXPECT_NSEQ(GetBrandedGoogleShieldSymbol(), consumer.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled ==
              delegate->message_state());
  EXPECT_FALSE(consumer.presentsModal);
}

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly
// creates a consented and flow disabled message prompt.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest,
       CheckConsentedAndFlowDisabledMessagePrompt) {
  std::unique_ptr<TailoredSecurityServiceInfobarDelegate> passed_delegate =
      MockTailoredSecurityServiceInfobarDelegate::Create(
          /*message_state*/ TailoredSecurityServiceMessageState::
              kConsentedAndFlowDisabled,
          nullptr);
  TailoredSecurityServiceInfobarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeTailoredSecurityService,
                     std::move(passed_delegate));
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      TailoredSecurityServiceBannerRequestConfig>(&infobar);
  TailoredSecurityInfobarBannerOverlayMediator* mediator =
      [[TailoredSecurityInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(CustomSymbolWithPointSize(kShieldSymbol, kSymbolImagePointSize),
              consumer.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled ==
              delegate->message_state());
}

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly
// creates an unconsented and flow enabled message prompt.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest,
       CheckUnconsentedAndFlowEnabledMessagePrompt) {
  std::unique_ptr<TailoredSecurityServiceInfobarDelegate> passed_delegate =
      MockTailoredSecurityServiceInfobarDelegate::Create(
          /*message_state*/ TailoredSecurityServiceMessageState::
              kUnconsentedAndFlowEnabled,
          nullptr);
  TailoredSecurityServiceInfobarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeTailoredSecurityService,
                     std::move(passed_delegate));
  std::unique_ptr<OverlayRequest> request = OverlayRequest::CreateWithConfig<
      TailoredSecurityServiceBannerRequestConfig>(&infobar);
  TailoredSecurityInfobarBannerOverlayMediator* mediator =
      [[TailoredSecurityInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(GetBrandedGoogleShieldSymbol(), consumer.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled ==
              delegate->message_state());
}
