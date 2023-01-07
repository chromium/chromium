// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/user_approved_account_list_manager.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UserApprovedAccountListManager::UserApprovedAccountListManager(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

UserApprovedAccountListManager::~UserApprovedAccountListManager() {}

std::vector<CoreAccountId>
UserApprovedAccountListManager::GetApprovedAccountIDList() const {
  DCHECK(pref_service_);
  const base::Value::List& accounts_pref =
      pref_service_->GetList(prefs::kSigninLastAccounts);
  std::vector<CoreAccountId> accounts;
  for (const auto& value : accounts_pref) {
    DCHECK(value.is_string());
    DCHECK(!value.GetString().empty());
    accounts.push_back(CoreAccountId::FromString(value.GetString()));
  }
  return accounts;
}

void UserApprovedAccountListManager::SetApprovedAccountList(
    const std::vector<CoreAccountInfo>& account_list) {
  DCHECK(pref_service_);
  DCHECK(!account_list.empty());
  base::Value::List accounts_pref_value;
  for (const CoreAccountInfo& account_info : account_list)
    accounts_pref_value.Append(account_info.account_id.ToString());
  pref_service_->SetList(prefs::kSigninLastAccounts,
                         std::move(accounts_pref_value));
}

void UserApprovedAccountListManager::ClearApprovedAccountList() {
  DCHECK(pref_service_);
  pref_service_->ClearPref(prefs::kSigninLastAccounts);
}

bool UserApprovedAccountListManager::IsAccountListApprouvedByUser(
    const std::vector<CoreAccountInfo>& current_accounts) const {
  DCHECK(!current_accounts.empty());
  // Sort the new account ID list.
  std::vector<CoreAccountId> current_account_id_list;
  for (const CoreAccountInfo& account_info : current_accounts)
    current_account_id_list.push_back(account_info.account_id);
  std::sort(current_account_id_list.begin(), current_account_id_list.end());
  // Sort the last approved account ID list.
  std::vector<CoreAccountId> last_approved_account_id_list =
      GetApprovedAccountIDList();
  std::sort(last_approved_account_id_list.begin(),
            last_approved_account_id_list.end());
  // Compare both.
  return last_approved_account_id_list == current_account_id_list;
}

void UserApprovedAccountListManager::Shutdown() {
  DCHECK(pref_service_);
  pref_service_ = nullptr;
}
