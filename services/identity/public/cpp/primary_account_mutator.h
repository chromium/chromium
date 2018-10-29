// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_H_

#include <string>

#include "base/callback_forward.h"
#include "components/signin/core/browser/signin_metrics.h"

struct AccountInfo;

namespace identity {

// PrimaryAccountMutator is the interface to set and clear the primary account
// (see IdentityManager for more information).
//
// It is a pure interface that has concrete implementation on platform that
// support changing the signed-in state during the lifetime of the application.
// On other platforms, there is no implementation, and no instance will be
// available at runtime (thus accessors may return null).
class PrimaryAccountMutator {
 public:
  // Represents the options for handling the accounts known to the
  // IdentityManager upon calling ClearPrimaryAccount().
  enum class ClearAccountsAction {
    kDefault,    // Default action based on internal policy.
    kKeepAll,    // Keep all accounts.
    kRemoveAll,  // Remove all accounts.
  };

  PrimaryAccountMutator() = default;
  virtual ~PrimaryAccountMutator() = default;

  // PrimaryAccountMutator is non-copyable, non-moveable.
  PrimaryAccountMutator(PrimaryAccountMutator&& other) = delete;
  PrimaryAccountMutator const& operator=(PrimaryAccountMutator&& other) =
      delete;

  PrimaryAccountMutator(const PrimaryAccountMutator& other) = delete;
  PrimaryAccountMutator const& operator=(const PrimaryAccountMutator& other) =
      delete;

  // Marks the account with |account_id| as the primary account, and returns
  // whether the operation succeeded or not. To succeed, this requires that:
  //    - setting the primary account is allowed,
  //    - the account username is allowed by policy,
  //    - the account is known by the IdentityManager.
  virtual bool SetPrimaryAccount(const std::string& account_id) = 0;

  // Clears the primary account. Depending on |action|, the other accounts
  // known to the IdentityManager may be deleted.
  virtual void ClearPrimaryAccount(
      ClearAccountsAction action,
      signin_metrics::ProfileSignout source_metric,
      signin_metrics::SignoutDelete delete_metric) = 0;

  // Getter and setter that allow enabling or disabling the ability to set the
  // primary account.
  virtual bool IsSettingPrimaryAccountAllowed() const = 0;
  virtual void SetSettingPrimaryAccountAllowed(bool allowed) = 0;

  // Getter and setter that allow enabling or disabling the ability to clear
  // the primary account.
  virtual bool IsClearingPrimaryAccountAllowed() const = 0;
  virtual void SetClearingPrimaryAccountAllowed(bool allowed) = 0;

  // Sets the pattern controlling which user names are allowed when setting
  // the primary account.
  virtual void SetAllowedPrimaryAccountPattern(const std::string& pattern) = 0;

  // All the following APIs are for use by legacy code only. They are deprecated
  // and should not be used when writing new code. They will be removed when the
  // old sign-in workflow has been turned down.

  // Attempts to sign-in user with a given refresh token and account. If it is
  // defined, |callback| should invoke either ClearPrimaryAccount() or
  // LegacyCompletePendingPrimaryAccountSignin() to either cancel or continue
  // the in progress sign-in (legacy, pre-DICE workflow).
  virtual void LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      const std::string& refresh_token,
      const std::string& gaia_id,
      const std::string& username,
      const std::string& password,
      base::RepeatingCallback<void(const std::string&)> callback) = 0;

  // Complete the in-process sign-in (legacy, pre-DICE workflow).
  virtual void LegacyCompletePendingPrimaryAccountSignin() = 0;

  // If applicable, merges the signed-in account into the cookie jar (legacy,
  // pre-DICE workflow).
  virtual void LegacyMergeSigninCredentialIntoCookieJar() = 0;

  // Returns true if there is a sign-in in progress (legacy, pre-DICE workflow).
  virtual bool LegacyIsPrimaryAccountAuthInProgress() const = 0;

  // If an authentication is in progress, returns the AccountInfo for the
  // account being authenticated. Returns an empty AccountInfo if no auth is
  // in progress (legacy, pre-DICE workflow).
  virtual AccountInfo LegacyPrimaryAccountForAuthInProgress() const = 0;

  // Copy auth credentials from the other PrimaryAccountMutator to this one.
  // Used when creating a new profile during the sign-in process to transfer
  // the in-progress credential information to the new profile (legacy, pre-
  // DICE workflow).
  virtual void LegacyCopyCredentialsFrom(
      const PrimaryAccountMutator& source) = 0;
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_MUTATOR_H_
