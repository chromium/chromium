// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_USER_APPROVED_ACCOUNT_LIST_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_USER_APPROVED_ACCOUNT_LIST_MANAGER_H_

#import "components/signin/public/identity_manager/account_info.h"

class PrefService;

// Manage the account list approval from the user. Accounts added/removed by
// other Google apps need to be approved by the user. The user can approve the
// current account list by the following actions in Chrome:
//  + sign-in.
//  + add/remove an account.
//  + confirm the SignedInAccountsViewController dialog.
class UserApprovedAccountListManager {
 public:
  explicit UserApprovedAccountListManager(PrefService* pref_service);
  UserApprovedAccountListManager(const UserApprovedAccountListManager&) =
      delete;
  UserApprovedAccountListManager& operator=(
      const UserApprovedAccountListManager&) = delete;
  ~UserApprovedAccountListManager();

  // Returns the last approved account ID list, or an empty list if
  // ClearApprovedAccounts().
  std::vector<CoreAccountId> GetApprovedAccountIDList() const;

  // Saves the new account list as being approved by the user. This method is
  // called when the user approve the current account list.
  // This method should be called only if there is a primary account.
  void SetApprovedAccountList(const std::vector<CoreAccountInfo>& account_list);

  // Clears the last approved account list. GetLastApprovedAccountIDList()
  // returns no value.
  // This method is called when the user signs out.
  void ClearApprovedAccountList();

  // Returns no value when there is no primary account, true if the user
  // approved the current account list.
  bool IsAccountListApprouvedByUser(
      const std::vector<CoreAccountInfo>& current_accounts) const;

  // Shutdowns this class. No methods should be invoked on this instance after
  // this call.
  void Shutdown();

 private:
  PrefService* pref_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_USER_APPROVED_ACCOUNT_LIST_MANAGER_H_
