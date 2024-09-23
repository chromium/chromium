// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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

// Test fixture for TailoredSecurityInfobarBannerOverlayMediator.
class TailoredSecurityInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  TailoredSecurityInfobarBannerOverlayMediatorTest() {}

  void InitInfobar(const TailoredSecurityServiceMessageState state) {
    std::unique_ptr<TailoredSecurityServiceInfobarDelegate> delegate =
        MockTailoredSecurityServiceInfobarDelegate::Create(state, nullptr);
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeTailoredSecurityService, std::move(delegate));
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    consumer_ = [[FakeInfobarBannerConsumer alloc] init];
    mediator_ = [[TailoredSecurityInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()];
    ;
    mediator_.consumer = consumer_;
  }

 protected:
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  raw_ptr<TailoredSecurityServiceInfobarDelegate> delegate_ = nil;
  FakeInfobarBannerConsumer* consumer_ = nil;
  TailoredSecurityInfobarBannerOverlayMediator* mediator_ = nil;
};

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly sets
// up its consumer.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  InitInfobar(TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled);

  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate_->GetMessageText());
  NSString* subtitle = base::SysUTF16ToNSString(delegate_->GetDescription());
  NSString* buttonText =
      base::SysUTF16ToNSString(delegate_->GetMessageActionText());
  NSString* bannerAccessibilityLabel =
      [NSString stringWithFormat:@"%@,%@", title, subtitle];
  EXPECT_NSEQ(bannerAccessibilityLabel, consumer_.bannerAccessibilityLabel);
  EXPECT_NSEQ(buttonText, consumer_.buttonText);
  EXPECT_NSEQ(title, consumer_.titleText);
  EXPECT_NSEQ(subtitle, consumer_.subtitleText);
  EXPECT_NSEQ(GetBrandedGoogleShieldSymbol(), consumer_.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kConsentedAndFlowEnabled ==
              delegate_->message_state());
  EXPECT_FALSE(consumer_.presentsModal);
}

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly
// creates a consented and flow disabled message prompt.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest,
       CheckConsentedAndFlowDisabledMessagePrompt) {
  InitInfobar(TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled);

  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(CustomSymbolWithPointSize(kShieldSymbol, kSymbolImagePointSize),
              consumer_.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kConsentedAndFlowDisabled ==
              delegate_->message_state());
}

// Tests that a TailoredSecurityInfobarBannerOverlayMediatorTest correctly
// creates an unconsented and flow enabled message prompt.
TEST_F(TailoredSecurityInfobarBannerOverlayMediatorTest,
       CheckUnconsentedAndFlowEnabledMessagePrompt) {
  InitInfobar(TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled);

  // Verify that the infobar was set up properly.
  EXPECT_NSEQ(GetBrandedGoogleShieldSymbol(), consumer_.iconImage);
  EXPECT_TRUE(TailoredSecurityServiceMessageState::kUnconsentedAndFlowEnabled ==
              delegate_->message_state());
}
