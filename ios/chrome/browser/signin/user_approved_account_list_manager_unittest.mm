// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/user_approved_account_list_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class UserApprovedAccountListManagerTest : public PlatformTest {
 protected:
  UserApprovedAccountListManagerTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();
  }

  // Returns AccountInfo1.
  CoreAccountInfo GetAccountInfo1() {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId("test1");
    account_info.gaia = "test1";
    account_info.email = "test1@gmail.com";
    return account_info;
  }

  // Returns AccountInfo2.
  CoreAccountInfo GetAccountInfo2() {
    CoreAccountInfo account_info;
    account_info.account_id = CoreAccountId::FromGaiaId("test2");
    account_info.gaia = "test2";
    account_info.email = "test2@gmail.com";
    return account_info;
  }

  // Returns a list with AccountInfo1.
  std::vector<CoreAccountInfo> GetAccountInfoListA() {
    std::vector<CoreAccountInfo> account_list;
    account_list.push_back(GetAccountInfo1());
    return account_list;
  }

  // Returns the sorted account id list for GetAccountInfoListA().
  std::vector<CoreAccountId> GetSortedAccountIdListA() {
    return SortAccountIdList(GetAccountInfoListA());
  }

  // Returns a list with AccountInfo1 and AccountInfo2.
  std::vector<CoreAccountInfo> GetAccountInfoListB() {
    std::vector<CoreAccountInfo> account_list;
    account_list.push_back(GetAccountInfo1());
    account_list.push_back(GetAccountInfo2());
    return account_list;
  }

  // Returns the sorted account id list for GetAccountInfoListB().
  std::vector<CoreAccountId> GetSortedAccountIdListB() {
    return SortAccountIdList(GetAccountInfoListB());
  }

  // Return the sorted account id list from an account info list.
  std::vector<CoreAccountId> SortAccountIdList(
      const std::vector<CoreAccountInfo>& account_list) {
    std::vector<CoreAccountId> account_id_list;
    for (const CoreAccountInfo& account_info : account_list)
      account_id_list.push_back(account_info.account_id);
    std::sort(account_id_list.begin(), account_id_list.end());
    return account_id_list;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that UserApprovedAccountListManager has an empty list after being
// initialized with empty prefs.
TEST_F(UserApprovedAccountListManagerTest, TestEmptyApprovedList) {
  UserApprovedAccountListManager manager(browser_state_->GetPrefs());
  const std::vector<CoreAccountInfo> empty_account_list;
  EXPECT_TRUE(manager.GetApprovedAccountIDList().empty());
}

// Tests the simple scenario by approving an account list and clearing it right
// after
TEST_F(UserApprovedAccountListManagerTest, TestApproveListAndClear) {
  UserApprovedAccountListManager manager(browser_state_->GetPrefs());
  // Approve account list.
  manager.SetApprovedAccountList(GetAccountInfoListA());
  EXPECT_EQ(GetSortedAccountIdListA(), manager.GetApprovedAccountIDList());
  EXPECT_TRUE(manager.IsAccountListApprouvedByUser(GetAccountInfoListA()));
  // Clear the list.
  manager.ClearApprovedAccountList();
  EXPECT_TRUE(manager.GetApprovedAccountIDList().empty());
}

// Tests:
//   + Approve A list
//   + Test A list
//   + Update to B list
//   + Approve B list
//   + Clear the list
TEST_F(UserApprovedAccountListManagerTest, TestApproveListAndUpdateList) {
  UserApprovedAccountListManager manager(browser_state_->GetPrefs());
  // Approve list A.
  manager.SetApprovedAccountList(GetAccountInfoListA());
  EXPECT_EQ(GetSortedAccountIdListA(), manager.GetApprovedAccountIDList());
  EXPECT_TRUE(manager.IsAccountListApprouvedByUser(GetAccountInfoListA()));
  // Test list B.
  EXPECT_FALSE(manager.IsAccountListApprouvedByUser(GetAccountInfoListB()));
  // Approve to list B.
  manager.SetApprovedAccountList(GetAccountInfoListB());
  EXPECT_EQ(GetSortedAccountIdListB(), manager.GetApprovedAccountIDList());
  EXPECT_TRUE(manager.IsAccountListApprouvedByUser(GetAccountInfoListB()));
  // Clear.
  manager.ClearApprovedAccountList();
  EXPECT_TRUE(manager.GetApprovedAccountIDList().empty());
}

// Tests GetApprovedAccountIDList() in the following scenario:
//   + UserApprovedAccountListManager being initialized
//   + Approve list A
//   + Test to list A
//   + Test to list B
//   + Approve list B
//   + Approved list cleared
TEST_F(UserApprovedAccountListManagerTest, GetApprovedAccountIDList) {
  UserApprovedAccountListManager manager(browser_state_->GetPrefs());
  EXPECT_TRUE(manager.GetApprovedAccountIDList().empty());
  // Approve list A
  manager.SetApprovedAccountList(GetAccountInfoListA());
  EXPECT_EQ(GetSortedAccountIdListA(), manager.GetApprovedAccountIDList());
  // Test List A
  EXPECT_TRUE(manager.IsAccountListApprouvedByUser(GetAccountInfoListA()));
  // Test List B
  EXPECT_FALSE(manager.IsAccountListApprouvedByUser(GetAccountInfoListB()));
  // Approve list A
  manager.SetApprovedAccountList(GetAccountInfoListB());
  EXPECT_EQ(GetSortedAccountIdListB(), manager.GetApprovedAccountIDList());
  // Clear list
  manager.ClearApprovedAccountList();
  EXPECT_TRUE(manager.GetApprovedAccountIDList().empty());
}
