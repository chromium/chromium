// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_
#define SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"

struct CoreAccountId;
struct CoreAccountInfo;

namespace signin {
struct AccessTokenInfo;
}

namespace identity {
struct AccountState;

class IdentityAccessorImpl : public mojom::IdentityAccessor,
                             public signin::IdentityManager::Observer {
 public:
  explicit IdentityAccessorImpl(signin::IdentityManager* identity_manager);
  ~IdentityAccessorImpl() override;

 private:
  // Map of outstanding access token requests.
  using AccessTokenFetchers =
      std::map<base::UnguessableToken,
               std::unique_ptr<signin::AccessTokenFetcher>>;

  // Invoked after access token request completes (successful or not).
  // Completes the pending access token request by calling back the consumer.
  void OnTokenRequestCompleted(
      base::UnguessableToken callback_id,
      scoped_refptr<base::RefCountedData<bool>> is_callback_done,
      GetAccessTokenCallback consumer_callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // mojom::IdentityAccessor:
  void GetPrimaryAccountInfo(GetPrimaryAccountInfoCallback callback) override;
  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override;
  void GetAccessToken(const CoreAccountId& account_id,
                      const ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override;

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;

  // Notified when there is a change in the state of the account
  // corresponding to |account_id|.
  void OnAccountStateChange(const CoreAccountId& account_id);

  // Gets the current state of the account represented by |account_info|.
  AccountState GetStateOfAccount(const CoreAccountInfo& account_info);

  signin::IdentityManager* identity_manager_;

  // The set of pending requests for access tokens.
  AccessTokenFetchers access_token_fetchers_;

  // List of callbacks that will be notified when the primary account is
  // available.
  std::vector<GetPrimaryAccountWhenAvailableCallback>
      primary_account_available_callbacks_;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_ACCESSOR_IMPL_H_
