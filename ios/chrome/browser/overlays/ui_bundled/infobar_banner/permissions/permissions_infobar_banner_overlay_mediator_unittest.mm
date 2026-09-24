// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Test fixture for PermissionsBannerOverlayMediator.
class PermissionsBannerOverlayMediatorTest : public PlatformTest {
 public:
  PermissionsBannerOverlayMediatorTest()
      : callback_installer_(
            &callback_receiver_,
            std::set<const OverlayResponseSupport*>(
                {InfobarBannerShowModalResponse::ResponseSupport()})) {}

 protected:
  MockOverlayRequestCallbackReceiver callback_receiver_;
  FakeOverlayRequestCallbackInstaller callback_installer_;
};

// Tests that a PermissionsBannerOverlayMediatorTest correctly sets up its
// consumer.
TEST_F(PermissionsBannerOverlayMediatorTest, SetUpConsumer) {
  NSArray<NSNumber*>* recently_accessible_permissions =
      @[ @(web::PermissionCamera), @(web::PermissionMicrophone) ];
  // Second parameter is used for modal; not needed for this test.
  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>(
          recently_accessible_permissions, nullptr);
  InfoBarIOS infobar(InfobarType::kInfobarTypePermissions, std::move(delegate));

  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kBanner);
  PermissionsBannerOverlayMediator* mediator =
      [[PermissionsBannerOverlayMediator alloc] initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_AND_MICROPHONE_ACCESSIBLE);
  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE),
              consumer.buttonText);
}

// Tests that tapping on the banner action presents the modal when domain level
// site permissions are disabled.
TEST_F(PermissionsBannerOverlayMediatorTest, PresentModal) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kDomainLevelSitePermissions);

  NSArray<NSNumber*>* recently_accessible_permissions =
      @[ @(web::PermissionCamera) ];
  // Second parameter is used for modal; not needed for this test.
  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>(
          recently_accessible_permissions, nullptr);
  InfoBarIOS infobar(InfobarType::kInfobarTypePermissions, std::move(delegate));

  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kBanner);
  callback_installer_.InstallCallbacks(request.get());
  PermissionsBannerOverlayMediator* mediator =
      [[PermissionsBannerOverlayMediator alloc] initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_ACCESSIBLE);
  EXPECT_NSEQ(title, consumer.titleText);
  // Check that the infobar modal is presented.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request.get(),
                       InfobarBannerShowModalResponse::ResponseSupport()));
  [mediator bannerInfobarButtonWasPressed:nil];
}

// Tests that tapping on the banner action shows the page action menu when
// domain level site permissions are enabled.
TEST_F(PermissionsBannerOverlayMediatorTest, ShowPageActionMenu) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kDomainLevelSitePermissions);

  NSArray<NSNumber*>* recently_accessible_permissions =
      @[ @(web::PermissionCamera) ];
  // Second parameter is used for modal; not needed for this test.
  std::unique_ptr<PermissionsInfobarDelegate> delegate =
      std::make_unique<PermissionsInfobarDelegate>(
          recently_accessible_permissions, nullptr);
  InfoBarIOS infobar(InfobarType::kInfobarTypePermissions, std::move(delegate));

  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<DefaultInfobarOverlayRequestConfig>(
          &infobar, InfobarOverlayType::kBanner);
  callback_installer_.InstallCallbacks(request.get());
  PermissionsBannerOverlayMediator* mediator =
      [[PermissionsBannerOverlayMediator alloc] initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;

  id mock_page_action_menu_handler =
      OCMStrictProtocolMock(@protocol(PageActionMenuCommands));
  mediator.pageActionMenuHandler = mock_page_action_menu_handler;

  OCMExpect([mock_page_action_menu_handler showPageActionMenu]);

  // Check that the infobar modal is NOT presented and page action menu is
  // shown.
  EXPECT_CALL(
      callback_receiver_,
      DispatchCallback(request.get(),
                       InfobarBannerShowModalResponse::ResponseSupport()))
      .Times(0);
  [mediator bannerInfobarButtonWasPressed:nil];

  EXPECT_OCMOCK_VERIFY(mock_page_action_menu_handler);
}
