// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"

#import <string>

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using confirm_infobar_overlays::ConfirmBannerRequestConfig;

// Test fixture for ConfirmInfobarBannerOverlayRequestConfig.
using ConfirmInfobarBannerOverlayRequestConfigTest = PlatformTest;

// Tests the ConfirmInfobarBannerOverlayRequestConfig constructor
// initilizes the members with the values provided by the delegate.
TEST_F(ConfirmInfobarBannerOverlayRequestConfigTest, Initilization) {
  // Create an InfoBarIOS with a ConfirmInfoBarDelegate.
  ui::ImageModel icon =
      ui::ImageModel::FromImage(gfx::Image([[UIImage alloc] init]));
  std::unique_ptr<FakeInfobarDelegate> passed_delegate =
      std::make_unique<FakeInfobarDelegate>(u"title", u"message",
                                            u"button label text", false, icon);
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm,
                     std::move(passed_delegate));
  infobar.set_high_priority(true);
  // Package the infobar into an OverlayRequest.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmBannerRequestConfig>(&infobar);
  ConfirmBannerRequestConfig* config =
      request->GetConfig<ConfirmBannerRequestConfig>();
  EXPECT_EQ(u"title", config->title_text());
  EXPECT_EQ(u"message", config->message_text());
  EXPECT_EQ(u"button label text", config->button_label_text());
  EXPECT_EQ(false, config->use_icon_background_tint());
  EXPECT_EQ(true, config->is_high_priority());
  EXPECT_EQ(icon.GetImage(), config->icon_image());
}

// Tests the ConfirmInfobarBannerOverlayRequestConfig constructor initilizes the
// members with default values.
TEST_F(ConfirmInfobarBannerOverlayRequestConfigTest,
       InitilizationWithTitleOnly) {
  // Create an InfoBarIOS with a ConfirmInfoBarDelegate.
  ui::ImageModel icon =
      ui::ImageModel::FromImage(gfx::Image([[UIImage alloc] init]));
  std::unique_ptr<FakeInfobarDelegate> passed_delegate =
      std::make_unique<FakeInfobarDelegate>(/* title */ u"",
                                            /* message text */ u"");
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm,
                     std::move(passed_delegate));
  // Package the infobar into an OverlayRequest.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmBannerRequestConfig>(&infobar);
  ConfirmBannerRequestConfig* config =
      request->GetConfig<ConfirmBannerRequestConfig>();
  EXPECT_EQ(u"", config->title_text());
  EXPECT_EQ(u"", config->message_text());
  EXPECT_EQ(u"", config->button_label_text());
  EXPECT_EQ(true, config->use_icon_background_tint());
  EXPECT_EQ(false, config->is_high_priority());
  EXPECT_EQ(true, config->icon_image().IsEmpty());
}
