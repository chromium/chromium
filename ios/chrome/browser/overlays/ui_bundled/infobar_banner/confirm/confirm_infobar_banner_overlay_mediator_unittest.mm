// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using confirm_infobar_overlays::ConfirmBannerRequestConfig;

// Test fixture for ConfirmInfobarBannerOverlayMediator.
using ConfirmInfobarBannerOverlayMediatorTest = PlatformTest;

// Tests that a ConfirmInfobarBannerOverlayMediator correctly sets up its
// consumer with a title and display message.
TEST_F(ConfirmInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithTitleAndMessage) {
  // Create an InfoBarIOS with a ConfirmInfoBarDelegate.
  std::unique_ptr<FakeInfobarDelegate> passed_delegate =
      std::make_unique<FakeInfobarDelegate>(u"title", u"message");
  FakeInfobarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmBannerRequestConfig>(&infobar);
  ConfirmInfobarBannerOverlayMediator* mediator =
      [[ConfirmInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate->GetTitleText());
  NSString* subtitle = base::SysUTF16ToNSString(delegate->GetMessageText());

  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(subtitle, consumer.subtitleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer.buttonText);
  EXPECT_FALSE(consumer.presentsModal);
}

// Tests that a ConfirmInfobarBannerOverlayMediator correctly sets up its
// consumer with a display message.
TEST_F(ConfirmInfobarBannerOverlayMediatorTest, SetUpConsumerWithMessage) {
  // Create an InfoBarIOS with a ConfirmInfoBarDelegate.
  std::unique_ptr<FakeInfobarDelegate> passed_delegate =
      std::make_unique<FakeInfobarDelegate>();
  FakeInfobarDelegate* delegate = passed_delegate.get();
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmBannerRequestConfig>(&infobar);
  ConfirmInfobarBannerOverlayMediator* mediator =
      [[ConfirmInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = base::SysUTF16ToNSString(delegate->GetMessageText());

  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(base::SysUTF16ToNSString(
                  delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK)),
              consumer.buttonText);
  EXPECT_FALSE(consumer.presentsModal);
}
