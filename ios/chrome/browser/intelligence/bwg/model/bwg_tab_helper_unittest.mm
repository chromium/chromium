// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class BwgTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    profile_->GetPrefs()->SetInteger(prefs::kGeminiEnabledByPolicy, 0);

    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());
    BwgTabHelper::CreateForWebState(web_state_.get());
    tab_helper_ = BwgTabHelper::FromWebState(web_state_.get());
  }

  bool IsBwgUiShowing() { return tab_helper_->is_bwg_ui_showing_; }

  // Environment objects are declared first, so they are destroyed last.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;

  // Profile and services that depend on the environment are declared next.
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  raw_ptr<BwgTabHelper> tab_helper_;
};

TEST_F(BwgTabHelperTest, TestSetBwgUiShowing) {
  ASSERT_FALSE(IsBwgUiShowing());
  tab_helper_->SetBwgUiShowing(true);
  ASSERT_TRUE(IsBwgUiShowing());
}

TEST_F(BwgTabHelperTest, TestGetIsBwgSessionActiveInBackground) {
  ASSERT_FALSE(tab_helper_->GetIsBwgSessionActiveInBackground());
  tab_helper_->SetBwgUiShowing(true);
  tab_helper_->WasHidden(web_state_.get());
  ASSERT_TRUE(tab_helper_->GetIsBwgSessionActiveInBackground());
}

// TODO(crbug.com/430313339): Add a test for the last interaction case.
TEST_F(BwgTabHelperTest, TestShouldShowSuggestionChips) {
  web_state_->SetCurrentURL(GURL("https://www.google.com/search?q=test"));
  ASSERT_FALSE(tab_helper_->ShouldShowSuggestionChips());

  web_state_->SetCurrentURL(GURL("https://www.not-google.com"));
  ASSERT_TRUE(tab_helper_->ShouldShowSuggestionChips());
}

TEST_F(BwgTabHelperTest, TestCreateOrUpdateBwgSessionInStorage) {
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  std::optional<std::string> retrieved_server_id = tab_helper_->GetServerId();
  ASSERT_TRUE(retrieved_server_id.has_value());
  ASSERT_EQ(server_id, retrieved_server_id.value());
}

TEST_F(BwgTabHelperTest, TestDeleteBwgSessionInStorage) {
  tab_helper_->CreateOrUpdateBwgSessionInStorage("test_server_id");
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  tab_helper_->DeleteBwgSessionInStorage();
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
}

TEST_F(BwgTabHelperTest, TestGetServerId) {
  ASSERT_FALSE(tab_helper_->GetServerId().has_value());
  std::string server_id = "test_server_id";
  tab_helper_->CreateOrUpdateBwgSessionInStorage(server_id);
  ASSERT_TRUE(tab_helper_->GetServerId().has_value());
  ASSERT_EQ(server_id, tab_helper_->GetServerId().value());
}
