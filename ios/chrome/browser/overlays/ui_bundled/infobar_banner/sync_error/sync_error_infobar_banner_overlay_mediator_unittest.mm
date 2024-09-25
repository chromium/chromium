// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/settings/model/sync/utils/test/mock_sync_error_infobar_delegate.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol SyncPresenter;

namespace {
const std::u16string kTitleText = u"title_text";
const std::u16string kMessageText = u"message_text";
const std::u16string kButtonLabelText = u"button_label_text";
}  // namespace

// Test fixture for SyncErrorInfobarBannerOverlayMediator.
class SyncErrorInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  SyncErrorInfobarBannerOverlayMediatorTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    // Create an InfoBarIOS with a MockSyncErrorInfobarDelegate.
    id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));
    std::unique_ptr<MockSyncErrorInfoBarDelegate> delegate =
        std::make_unique<MockSyncErrorInfoBarDelegate>(
            profile_.get(), presenter, kTitleText, kMessageText,
            kButtonLabelText,
            /*use_icon_background_tint=*/true);
    // Create an InfoBarIOS with a MockSyncErrorInfoBarDelegate.
    delegate_ = delegate.get();
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSyncError,
                                            std::move(delegate));
    // Package the infobar into an OverlayRequest, then create a mediator that
    // uses this request in order to set up a fake consumer.
    request_ =
        OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
            infobar_.get(), InfobarOverlayType::kBanner);
    mediator_ = [[SyncErrorInfobarBannerOverlayMediator alloc]
        initWithRequest:request_.get()];
    consumer_mock_ = OCMProtocolMock(@protocol(InfobarBannerConsumer));
  }

  ~SyncErrorInfobarBannerOverlayMediatorTest() override {
    // Force the mediator to be deallocated before the
    // request is destroyed to avoid undefined behaviour.
    @autoreleasepool {
      mediator_ = nil;
    }
  }

 protected:
  raw_ptr<MockSyncErrorInfoBarDelegate> delegate_ = nil;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<InfoBarIOS> infobar_;
  std::unique_ptr<OverlayRequest> request_;
  SyncErrorInfobarBannerOverlayMediator* mediator_;
  id consumer_mock_;
};

// Tests that a SyncErrorInfobarBannerOverlayMediator correctly sets up its
// consumer with correct messages.
TEST_F(SyncErrorInfobarBannerOverlayMediatorTest, SetUpConsumerWithMessages) {
  mediator_.consumer = consumer_mock_;
  // Verify that the infobar's text fields was set up properly.
  OCMExpect([consumer_mock_ setTitleText:base::SysUTF16ToNSString(kTitleText)]);
  OCMExpect(
      [consumer_mock_ setSubtitleText:base::SysUTF16ToNSString(kMessageText)]);
  OCMExpect([consumer_mock_
      setButtonText:base::SysUTF16ToNSString(kButtonLabelText)]);
}

// Tests that a SyncErrorInfobarBannerOverlayMediator correctly sets up its
// consumer's icon using SF symbol.
TEST_F(SyncErrorInfobarBannerOverlayMediatorTest,
       SetUpConsumerWithIconSettingsUseSFSymbol) {
  mediator_.consumer = consumer_mock_;
  // Verify that the infobar's icon was set up properly.
  OCMExpect([consumer_mock_
      setIconImageTintColor:[UIColor colorNamed:kTextPrimaryColor]]);
  OCMExpect([consumer_mock_
      setIconBackgroundColor:[UIColor colorNamed:kRed500Color]]);
  OCMExpect([consumer_mock_ setUseIconBackgroundTint:true]);
}

// Tests that when the main button is pressed it calls 'Accept()'.
TEST_F(SyncErrorInfobarBannerOverlayMediatorTest, MainAction) {
  mediator_.consumer = consumer_mock_;

  // Verify that the 'Accept()' method is called.
  EXPECT_CALL(*delegate_, Accept());
  [mediator_ bannerInfobarButtonWasPressed:nil];
}

// Ensures that calling the -bannerInfobarButtonWasPressed: after the infobar
// has been removed does not cause a crash. This could happen if the infobar is
// removed before the banner has finished appearing.
TEST_F(SyncErrorInfobarBannerOverlayMediatorTest,
       BannerInfobarButtonWasPressedAfterRemoval) {
  // Removes the infobar.
  infobar_ = nullptr;

  [mediator_ bannerInfobarButtonWasPressed:nil];
}
