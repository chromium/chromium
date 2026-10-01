// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator_factory.h"

#import <memory>

#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using confirm_infobar_overlays::ConfirmBannerRequestConfig;
using InfobarBannerOverlayMediatorFactoryTest = PlatformTest;

// Tests that requestSupport returns non-null and supports expected configs.
TEST_F(InfobarBannerOverlayMediatorFactoryTest, RequestSupport) {
  const OverlayRequestSupport* support =
      [InfobarBannerOverlayMediator requestSupport];
  ASSERT_TRUE(support);

  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>(@[], nullptr);
  InfoBarIOS permissions_infobar(InfobarType::kInfobarTypePermissions,
                                 std::move(delegate));
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          &permissions_infobar, InfobarOverlayType::kBanner);
  EXPECT_TRUE(support->IsRequestSupported(request.get()));
}

// Tests that mediatorForRequest: returns the correct mediator for a default
// infobar request.
TEST_F(InfobarBannerOverlayMediatorFactoryTest, MediatorForDefaultRequest) {
  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>(@[], nullptr);
  InfoBarIOS permissions_infobar(InfobarType::kInfobarTypePermissions,
                                 std::move(delegate));
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          &permissions_infobar, InfobarOverlayType::kBanner);
  InfobarBannerOverlayMediator* mediator =
      [InfobarBannerOverlayMediator mediatorForRequest:request.get()];
  EXPECT_TRUE(
      [mediator isKindOfClass:[PermissionsBannerOverlayMediator class]]);
}

// Tests that mediatorForRequest: returns the correct mediator for a custom
// infobar request.
TEST_F(InfobarBannerOverlayMediatorFactoryTest, MediatorForCustomRequest) {
  InfoBarIOS confirm_infobar(InfobarType::kInfobarTypeConfirm,
                             std::make_unique<FakeInfobarDelegate>());
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<ConfirmBannerRequestConfig>(
          &confirm_infobar);
  InfobarBannerOverlayMediator* mediator =
      [InfobarBannerOverlayMediator mediatorForRequest:request.get()];
  EXPECT_TRUE(
      [mediator isKindOfClass:[ConfirmInfobarBannerOverlayMediator class]]);
}
