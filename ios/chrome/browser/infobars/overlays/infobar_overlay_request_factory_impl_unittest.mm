// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory_impl.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/password_form.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_feature.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/update_password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/passwords/ios_chrome_save_password_infobar_delegate.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/test/fake_infobar_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBar;
using infobars::InfoBarDelegate;

// Test fixture for InfobarOverlayRequestFactoryImpl.
class InfobarOverlayRequestFactoryImplTest : public PlatformTest {
 public:
  InfobarOverlayRequestFactoryImplTest() {
    feature_list_.InitWithFeatures({kIOSInfobarUIReboot},
                                   {kInfobarUIRebootOnlyiOS13});
  }

  InfobarOverlayRequestFactory* factory() { return &factory_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  InfobarOverlayRequestFactoryImpl factory_;
};

// Tests that the factory creates a save passwords infobar request.
TEST_F(InfobarOverlayRequestFactoryImplTest, SavePasswords) {
  FakeInfobarUIDelegate* ui_delegate = [[FakeInfobarUIDelegate alloc] init];
  ui_delegate.infobarType = InfobarType::kInfobarTypePasswordSave;
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(ui_delegate, std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(banner_request
                  ->GetConfig<SavePasswordInfobarBannerOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(
      modal_request->GetConfig<PasswordInfobarModalOverlayRequestConfig>());
}

// Tests that the factory creates an update passwords infobar request.
TEST_F(InfobarOverlayRequestFactoryImplTest, UpdatePasswords) {
  GURL url("https://chromium.test");
  std::unique_ptr<InfoBarDelegate> delegate =
      MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username", @"password",
                                                       url);
  InfoBarIOS infobar(InfobarType::kInfobarTypePasswordUpdate,
                     std::move(delegate));

  // Test banner request creation.
  std::unique_ptr<OverlayRequest> banner_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(
      banner_request
          ->GetConfig<UpdatePasswordInfobarBannerOverlayRequestConfig>());

  // Test modal request creation.
  std::unique_ptr<OverlayRequest> modal_request =
      factory()->CreateInfobarRequest(&infobar, InfobarOverlayType::kModal);
  EXPECT_TRUE(
      modal_request->GetConfig<PasswordInfobarModalOverlayRequestConfig>());
  // TODO(crbug.com/1033154): Add additional tests for other
  // InfobarOverlayTypes.
}
