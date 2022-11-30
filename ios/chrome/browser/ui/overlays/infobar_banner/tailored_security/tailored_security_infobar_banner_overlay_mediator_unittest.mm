// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/test/mock_tailored_security_service_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
              kConsentedAndFlowEnabled);
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
  EXPECT_NSEQ([UIImage imageNamed:@"icon_image"], consumer.iconImage);
  EXPECT_TRUE(consumer.presentsModal);
}
