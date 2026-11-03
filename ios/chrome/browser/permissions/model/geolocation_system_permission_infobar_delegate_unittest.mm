// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/model/geolocation_system_permission_infobar_delegate.h"

#import <memory>

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/geolocation_prompt_dialog_overlay.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

class GeolocationSystemPermissionInfoBarDelegateTest : public PlatformTest {
 public:
  GeolocationSystemPermissionInfoBarDelegateTest() {
    profile_ = TestProfileIOS::Builder().Build();
    web_state_.SetBrowserState(profile_.get());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    OverlayRequestQueue::CreateForWebState(&web_state_);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  web::FakeWebState& web_state() { return web_state_; }

  ConfirmInfoBarDelegate* CreateAndGetDelegate(
      web::WebStatePermissionDecisionHandler handler) {
    if (!GeolocationSystemPermissionInfoBarDelegate::Create(&web_state_,
                                                            handler)) {
      return nullptr;
    }
    infobars::InfoBarManager* manager =
        InfoBarManagerImpl::FromWebState(&web_state_);
    if (!manager || manager->infobars().empty()) {
      return nullptr;
    }
    return manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
};

// Tests that the delegate returns the expected UI properties.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest, UIProperties) {
  base::test::TestFuture<web::PermissionDecision> decision_future;
  ConfirmInfoBarDelegate* delegate = CreateAndGetDelegate(
      base::CallbackToBlock(decision_future.GetCallback()));
  ASSERT_NE(nullptr, delegate);

  EXPECT_EQ(delegate->GetIdentifier(),
            infobars::InfoBarDelegate::IOS_PERMISSIONS_INFOBAR_DELEGATE);
  EXPECT_EQ(delegate->GetTitleText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_BANNER_TITLE));
  EXPECT_EQ(delegate->GetMessageText(),
            l10n_util::GetStringUTF16(
                IDS_IOS_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_BANNER_MESSAGE));
  EXPECT_EQ(delegate->GetButtons(), ConfirmInfoBarDelegate::BUTTON_OK);
  EXPECT_EQ(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
            l10n_util::GetStringUTF16(IDS_CONTINUE));
  EXPECT_FALSE(delegate->GetIcon().IsEmpty());
}

// Tests that Create adds the infobar to the InfoBarManager.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest, CreateSuccess) {
  base::test::TestFuture<web::PermissionDecision> decision_future;
  bool result = GeolocationSystemPermissionInfoBarDelegate::Create(
      &web_state(), base::CallbackToBlock(decision_future.GetCallback()));

  EXPECT_TRUE(result);
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(&web_state());
  ASSERT_TRUE(manager);
  EXPECT_EQ(1U, manager->infobars().size());
}

// Tests that Create fails gracefully and invokes the handler with Deny if
// InfoBarManager is missing.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest,
       CreateFailsWithoutInfoBarManager) {
  web::FakeWebState web_state_without_manager;
  base::test::TestFuture<web::PermissionDecision> decision_future;
  bool result = GeolocationSystemPermissionInfoBarDelegate::Create(
      &web_state_without_manager,
      base::CallbackToBlock(decision_future.GetCallback()));

  EXPECT_FALSE(result);
  EXPECT_EQ(web::PermissionDecisionDeny, decision_future.Get());
}

// Tests that Accept() queues a GeolocationPromptDialogRequest.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest, AcceptQueuesOverlay) {
  base::test::TestFuture<web::PermissionDecision> decision_future;
  ConfirmInfoBarDelegate* delegate = CreateAndGetDelegate(
      base::CallbackToBlock(decision_future.GetCallback()));
  ASSERT_NE(nullptr, delegate);

  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state(), OverlayModality::kWebContentArea);
  ASSERT_TRUE(queue);
  EXPECT_EQ(0U, queue->size());

  EXPECT_TRUE(delegate->Accept());
  EXPECT_EQ(1U, queue->size());
  EXPECT_TRUE(
      queue->front_request()->GetConfig<GeolocationPromptDialogRequest>());

  // The decision should not be resolved yet until the dialog completes.
  EXPECT_FALSE(decision_future.IsReady());
}

// Tests that InfoBarDismissed() resolves the decision handler with Deny.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest, InfoBarDismissedDenies) {
  base::test::TestFuture<web::PermissionDecision> decision_future;
  ConfirmInfoBarDelegate* delegate = CreateAndGetDelegate(
      base::CallbackToBlock(decision_future.GetCallback()));
  ASSERT_NE(nullptr, delegate);

  delegate->InfoBarDismissed();
  EXPECT_EQ(web::PermissionDecisionDeny, decision_future.Get());
}

// Tests that destroying the delegate without action resolves with Deny.
TEST_F(GeolocationSystemPermissionInfoBarDelegateTest,
       DestructionDispatchesDeny) {
  base::test::TestFuture<web::PermissionDecision> decision_future;
  ASSERT_TRUE(GeolocationSystemPermissionInfoBarDelegate::Create(
      &web_state(), base::CallbackToBlock(decision_future.GetCallback())));
  EXPECT_FALSE(decision_future.IsReady());

  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(&web_state());
  ASSERT_TRUE(manager);
  manager->RemoveAllInfoBars(/*animate=*/false);
  EXPECT_EQ(web::PermissionDecisionDeny, decision_future.Get());
}
