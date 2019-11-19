// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/identity_accessor_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/account_state.h"

namespace identity {

void IdentityAccessorImpl::OnTokenRequestCompleted(
    base::UnguessableToken callback_id,
    scoped_refptr<base::RefCountedData<bool>> is_callback_done,
    GetAccessTokenCallback consumer_callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() == GoogleServiceAuthError::NONE) {
    std::move(consumer_callback)
        .Run(access_token_info.token, access_token_info.expiration_time, error);
  } else {
    std::move(consumer_callback).Run(base::nullopt, base::Time(), error);
  }

  is_callback_done->data = true;
  access_token_fetchers_.erase(callback_id);
}

IdentityAccessorImpl::IdentityAccessorImpl(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);
}

IdentityAccessorImpl::~IdentityAccessorImpl() {
  identity_manager_->RemoveObserver(this);
}

void IdentityAccessorImpl::GetPrimaryAccountInfo(
    GetPrimaryAccountInfoCallback callback) {
  CoreAccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);
  std::move(callback).Run(account_info.account_id, account_info.gaia,
                          account_info.email, account_state);
}

void IdentityAccessorImpl::GetPrimaryAccountWhenAvailable(
    GetPrimaryAccountWhenAvailableCallback callback) {
  CoreAccountInfo account_info = identity_manager_->GetPrimaryAccountInfo();
  AccountState account_state = GetStateOfAccount(account_info);

  if (!account_state.has_refresh_token ||
      identity_manager_->GetErrorStateOfRefreshTokenForAccount(
          account_info.account_id) != GoogleServiceAuthError::AuthErrorNone()) {
    primary_account_available_callbacks_.push_back(std::move(callback));
    return;
  }

  DCHECK(!account_info.account_id.empty());
  DCHECK(!account_info.email.empty());
  DCHECK(!account_info.gaia.empty());
  std::move(callback).Run(account_info.account_id, account_info.gaia,
                          account_info.email, account_state);
}

void IdentityAccessorImpl::GetAccessToken(const CoreAccountId& account_id,
                                          const ScopeSet& scopes,
                                          const std::string& consumer_id,
                                          GetAccessTokenCallback callback) {
  base::UnguessableToken callback_id = base::UnguessableToken::Create();
  auto is_callback_done =
      base::MakeRefCounted<base::RefCountedData<bool>>(false);

  std::unique_ptr<signin::AccessTokenFetcher> fetcher =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          account_id, consumer_id, scopes,
          base::BindOnce(&IdentityAccessorImpl::OnTokenRequestCompleted,
                         base::Unretained(this), callback_id, is_callback_done,
                         std::move(callback)),
          signin::AccessTokenFetcher::Mode::kImmediate);

  // If our callback hasn't already been run, hold on to the AccessTokenFetcher
  // so it won't be cleaned up until the request is done.
  if (!is_callback_done->data) {
    access_token_fetchers_[callback_id] = std::move(fetcher);
  }
}

void IdentityAccessorImpl::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  OnAccountStateChange(account_info.account_id);
}

void IdentityAccessorImpl::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  OnAccountStateChange(primary_account_info.account_id);
}

void IdentityAccessorImpl::OnAccountStateChange(
    const CoreAccountId& account_id) {
  base::Optional<AccountInfo> account_info =
      identity_manager_
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByAccountId(
              account_id);
  if (account_info.has_value()) {
    AccountState account_state = GetStateOfAccount(account_info.value());

    // Check whether the primary account is available and notify any waiting
    // consumers if so.
    if (account_state.is_primary_account &&
        !identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
            account_info->account_id)) {
      DCHECK(!account_info->account_id.empty());
      DCHECK(!account_info->email.empty());
      DCHECK(!account_info->gaia.empty());

      for (auto&& callback : primary_account_available_callbacks_) {
        std::move(callback).Run(account_info->account_id, account_info->gaia,
                                account_info->email, account_state);
      }
      primary_account_available_callbacks_.clear();
    }
  }
}

AccountState IdentityAccessorImpl::GetStateOfAccount(
    const CoreAccountInfo& account_info) {
  AccountState account_state;
  account_state.has_refresh_token =
      identity_manager_->HasAccountWithRefreshToken(account_info.account_id);
  account_state.is_primary_account =
      (account_info.account_id == identity_manager_->GetPrimaryAccountId());
  return account_state;
}

}  // namespace identity
