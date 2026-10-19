// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_browser_agent.h"

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test fixture for `DataProtectionBrowserAgent`.
class DataProtectionBrowserAgentTest : public PlatformTest {
 public:
  DataProtectionBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    DataProtectionBrowserAgent::CreateForBrowser(browser_.get());
  }

  ~DataProtectionBrowserAgentTest() override = default;

  // Creates a fake WebState configured with required tab helpers for testing.
  std::unique_ptr<web::FakeWebState> CreateWebState() {
    auto web_state =
        std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
    web_state->SetBrowserState(profile_.get());
    OverlayRequestQueue::CreateForWebState(web_state.get());
    DataProtectionTabHelper::CreateForWebState(web_state.get());
    return web_state;
  }

  // Updates the committed protection state on `web_state`'s tab helper.
  void SetCommittedProtectionState(web::WebState* web_state,
                                   const std::string& watermark_text) {
    DataProtectionTabHelper* helper =
        DataProtectionTabHelper::FromWebState(web_state);
    ASSERT_TRUE(helper);
    helper->SetCommittedProtectionState(
        DataProtectionTabHelper::ProtectionState(
            DataProtectionTabHelper::Enabled{}),
        watermark_text);
  }

  // Returns the profile's watermark tabs preference dictionary.
  const base::DictValue& GetWatermarkedTabsPref() const {
    return profile_->GetPrefs()->GetDict(
        enterprise_data_protection::kDataProtectionWatermarkedTabs);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests that when a tab's watermark changes, the pref dictionary is updated.
TEST_F(DataProtectionBrowserAgentTest, WatermarkPrefUpdatedOnStateChange) {
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const std::string tab_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());
  EXPECT_FALSE(GetWatermarkedTabsPref().contains(tab_id));

  // Set watermark and verify pref updated.
  SetCommittedProtectionState(web_state, "Confidential");
  const std::string* watermark = GetWatermarkedTabsPref().FindString(tab_id);
  ASSERT_TRUE(watermark);
  EXPECT_EQ(*watermark, "Confidential");

  // Change watermark and verify pref updated.
  SetCommittedProtectionState(web_state, "Top Secret");
  watermark = GetWatermarkedTabsPref().FindString(tab_id);
  ASSERT_TRUE(watermark);
  EXPECT_EQ(*watermark, "Top Secret");

  // Clear watermark and verify key removed from pref.
  SetCommittedProtectionState(web_state, "");
  EXPECT_FALSE(GetWatermarkedTabsPref().contains(tab_id));
}

// Tests that when a tab is closed, its entry in prefs is deleted.
TEST_F(DataProtectionBrowserAgentTest, WatermarkPrefRemovedOnTabClose) {
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const std::string tab_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());
  SetCommittedProtectionState(web_state, "Confidential");
  EXPECT_TRUE(GetWatermarkedTabsPref().contains(tab_id));

  // Close the tab by user action.
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);
  EXPECT_FALSE(GetWatermarkedTabsPref().contains(tab_id));
}

// Tests that when a tab is closed for cleanup, its entry in prefs is deleted.
TEST_F(DataProtectionBrowserAgentTest, WatermarkPrefRemovedOnTabsCleanup) {
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const std::string tab_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());
  SetCommittedProtectionState(web_state, "Confidential");
  EXPECT_TRUE(GetWatermarkedTabsPref().contains(tab_id));

  // Close the tab by tabs cleanup.
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kTabsCleanup);
  EXPECT_FALSE(GetWatermarkedTabsPref().contains(tab_id));
}

// Tests that when a tab is detached without being closed (e.g. shutdown /
// moving windows), its entry in prefs is preserved.
TEST_F(DataProtectionBrowserAgentTest,
       WatermarkPrefPreservedOnDetachWithoutClose) {
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const std::string tab_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());
  SetCommittedProtectionState(web_state, "Confidential");
  EXPECT_TRUE(GetWatermarkedTabsPref().contains(tab_id));

  // Detach the web state without closing.
  std::unique_ptr<web::WebState> detached =
      browser_->GetWebStateList()->DetachWebStateAt(0);

  // The entry must still be preserved in prefs.
  EXPECT_TRUE(GetWatermarkedTabsPref().contains(tab_id));
}
