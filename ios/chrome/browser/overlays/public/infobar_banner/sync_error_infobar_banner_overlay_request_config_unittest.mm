// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_banner/sync_error_infobar_banner_overlay_request_config.h"

#import <UIKit/UIKit.h>
#import <string>

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/settings/sync/utils/test/mock_sync_error_infobar_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using sync_error_infobar_overlays::SyncErrorBannerRequestConfig;

namespace {
const std::u16string kTitleText = u"title_text";
const std::u16string kMessageText = u"message_text";
const std::u16string kButtonLabelText = u"button_label_text";
}  // namespace

// Test fixture for SyncErrorInfobarBannerOverlayRequestConfig.
class SyncErrorInfobarBannerOverlayRequestConfigTest : public PlatformTest {
 public:
  SyncErrorInfobarBannerOverlayRequestConfigTest() {
    TestChromeBrowserState::Builder builder;
    chrome_browser_state_ = builder.Build();

    // Create an InfoBarIOS with a MockSyncErrorInfobarDelegate.
    id presenter = OCMStrictProtocolMock(@protocol(SyncPresenter));

    std::unique_ptr<MockSyncErrorInfoBarDelegate> delegate =
        std::make_unique<MockSyncErrorInfoBarDelegate>(
            chrome_browser_state_.get(), presenter, kTitleText, kMessageText,
            kButtonLabelText,
            /*use_icon_background_tint=*/true);
    infobar_ = std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeSyncError,
                                            std::move(delegate));
  }

 protected:
  web::WebTaskEnvironment task_environment;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<InfoBarIOS> infobar_;
};

// Tests the SyncErrorInfobarBannerOverlayRequestConfig then SF symbol is
// enabled.
TEST_F(SyncErrorInfobarBannerOverlayRequestConfigTest, IconConfigsForSFSymbol) {
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<SyncErrorBannerRequestConfig>(
          infobar_.get());
  SyncErrorBannerRequestConfig* config =
      request->GetConfig<SyncErrorBannerRequestConfig>();

  EXPECT_NSEQ([UIColor colorNamed:kPrimaryBackgroundColor],
              config -> icon_image_tint_color());
  EXPECT_NSEQ([UIColor colorNamed:kRed500Color],
              config -> background_tint_color());
  EXPECT_EQ(true, config->use_icon_background_tint());
  EXPECT_EQ(kTitleText, config->title_text());
  EXPECT_EQ(kMessageText, config->message_text());
  EXPECT_EQ(kButtonLabelText, config->button_label_text());
}
