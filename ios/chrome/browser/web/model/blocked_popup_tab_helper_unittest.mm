// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/blocked_popup_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/infobars/model/confirm_infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using web::WebState;

// Test fixture for BlockedPopupTabHelper class.
class BlockedPopupTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
    web_state_->SetDelegate(&web_state_delegate_);

    BlockedPopupTabHelper::GetOrCreateForWebState(web_state());
    InfoBarManagerImpl::CreateForWebState(web_state());
  }

  // Returns true if InfoBarManager is being observed.
  bool IsObservingSources() {
    return GetBlockedPopupTabHelper()->scoped_observation_.IsObserving();
  }

  // Returns BlockedPopupTabHelper that is being tested.
  BlockedPopupTabHelper* GetBlockedPopupTabHelper() {
    return BlockedPopupTabHelper::GetOrCreateForWebState(web_state());
  }

  // Returns InfoBarManager attached to `web_state()`.
  infobars::InfoBarManager* GetInfobarManager() {
    return InfoBarManagerImpl::FromWebState(web_state());
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;
  web::FakeWebStateDelegate web_state_delegate_;
};

// Tests ShouldBlockPopup method. This test changes content settings without
// restoring them back, which is fine because changes do not persist across test
// runs.
TEST_F(BlockedPopupTabHelperTest, ShouldBlockPopup) {
  const GURL source_url1("https://source-url1");
  EXPECT_TRUE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url1));

  // Allow popups for `source_url1`.
  scoped_refptr<HostContentSettingsMap> settings_map(
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get()));
  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromURL(source_url1),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::POPUPS,
      CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url1));
  const GURL source_url2("https://source-url2");
  EXPECT_TRUE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url2));

  // Allow all popups.
  settings_map->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                         CONTENT_SETTING_ALLOW);

  EXPECT_FALSE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url1));
  EXPECT_FALSE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url2));
}

// Tests that allowing blocked popup opens a child window and allows future
// popups for the source url.
TEST_F(BlockedPopupTabHelperTest, AllowBlockedPopup) {
  const GURL source_url("https://source-url");
  ASSERT_TRUE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url));

  // Block popup.
  const GURL target_url("https://target-url");
  web::Referrer referrer(source_url, web::ReferrerPolicyDefault);
  GetBlockedPopupTabHelper()->HandlePopup(target_url, referrer);

  // Allow blocked popup.
  ASSERT_EQ(1U, GetInfobarManager()->infobars().size());
  infobars::InfoBar* infobar = GetInfobarManager()->infobars()[0];
  auto* delegate = infobar->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  ASSERT_FALSE(web_state_delegate_.last_open_url_request());
  delegate->Accept();

  // Verify that popups are allowed for `test_url`.
  EXPECT_FALSE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url));

  // Verify that child window was open.
  auto* open_url_request = web_state_delegate_.last_open_url_request();
  ASSERT_TRUE(open_url_request);
  EXPECT_EQ(web_state(), open_url_request->web_state);
  WebState::OpenURLParams params = open_url_request->params;
  EXPECT_EQ(target_url, params.url);
  EXPECT_EQ(source_url, params.referrer.url);
  EXPECT_EQ(web::ReferrerPolicyDefault, params.referrer.policy);
  EXPECT_EQ(WindowOpenDisposition::NEW_POPUP, params.disposition);
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(params.transition, ui::PAGE_TRANSITION_LINK));
  EXPECT_TRUE(params.is_renderer_initiated);
}

// Tests that destroying WebState while Infobar is presented does not crash.
TEST_F(BlockedPopupTabHelperTest, DestroyWebState) {
  const GURL source_url("https://source-url");
  ASSERT_TRUE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url));

  // Block popup.
  const GURL target_url("https://target-url");
  web::Referrer referrer(source_url, web::ReferrerPolicyDefault);
  GetBlockedPopupTabHelper()->HandlePopup(target_url, referrer);

  // Verify that destroying WebState does not crash.
  web_state_.reset();
}

// Tests that an infobar is added to the infobar manager when
// BlockedPopupTabHelper::HandlePopup() is called.
TEST_F(BlockedPopupTabHelperTest, ShowAndDismissInfoBar) {
  // Check that there are no infobars showing and no registered observers.
  EXPECT_EQ(0U, GetInfobarManager()->infobars().size());
  EXPECT_FALSE(IsObservingSources());

  // Call `HandlePopup` to show an infobar.
  const GURL test_url("https://popups.example.com");
  GetBlockedPopupTabHelper()->HandlePopup(test_url, web::Referrer());
  ASSERT_EQ(1U, GetInfobarManager()->infobars().size());
  EXPECT_TRUE(IsObservingSources());

  // Dismiss the infobar and check that the tab helper no longer has any
  // registered observers.
  GetInfobarManager()->infobars()[0]->RemoveSelf();
  EXPECT_EQ(0U, GetInfobarManager()->infobars().size());
  EXPECT_FALSE(IsObservingSources());
}

// Tests that the Infobar presentation and dismissal histograms are recorded
// correctly.
TEST_F(BlockedPopupTabHelperTest, RecordDismissMetrics) {
  base::HistogramTester histogram_tester;

  // Call `HandlePopup` to show an infobar and check that the Presented
  // histogram was recorded correctly.
  const GURL test_url("https://popups.example.com");
  GetBlockedPopupTabHelper()->HandlePopup(test_url, web::Referrer());
  ASSERT_EQ(1U, GetInfobarManager()->infobars().size());
  histogram_tester.ExpectUniqueSample(
      "Mobile.Messages.Confirm.Event.ConfirmInfobarTypeBlockPopups",
      static_cast<base::HistogramBase::Sample>(
          MobileMessagesConfirmInfobarEvents::Presented),
      1);

  // Dismiss the infobar and check that the Dismiss histogram was recorded
  // correctly.
  GetInfobarManager()->infobars()[0]->delegate()->InfoBarDismissed();
  histogram_tester.ExpectBucketCount(
      kInfobarTypeBlockPopupsEventHistogram,
      static_cast<base::HistogramBase::Sample>(
          MobileMessagesConfirmInfobarEvents::Dismissed),
      1);
}

// Tests that the Infobar accept histogram is recorded correctly.
TEST_F(BlockedPopupTabHelperTest, RecordAcceptMetrics) {
  base::HistogramTester histogram_tester;
  const GURL source_url("https://source-url");
  ASSERT_TRUE(GetBlockedPopupTabHelper()->ShouldBlockPopup(source_url));

  // Block popup.
  const GURL target_url("https://target-url");
  web::Referrer referrer(source_url, web::ReferrerPolicyDefault);
  GetBlockedPopupTabHelper()->HandlePopup(target_url, referrer);

  // Accept the infobar and check that the Accepted histogram was recorded
  // correctly.
  ASSERT_EQ(1U, GetInfobarManager()->infobars().size());
  auto* delegate = GetInfobarManager()
                       ->infobars()[0]
                       ->delegate()
                       ->AsConfirmInfoBarDelegate();
  delegate->Accept();
  histogram_tester.ExpectBucketCount(
      kInfobarTypeBlockPopupsEventHistogram,
      static_cast<base::HistogramBase::Sample>(
          MobileMessagesConfirmInfobarEvents::Accepted),
      1);
}
