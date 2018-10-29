// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator_impl.h"

#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"

namespace identity {

PrimaryAccountMutatorImpl::PrimaryAccountMutatorImpl(
    AccountTrackerService* account_tracker,
    SigninManager* signin_manager)
    : account_tracker_(account_tracker), signin_manager_(signin_manager) {
  DCHECK(account_tracker_);
  DCHECK(signin_manager_);
}

PrimaryAccountMutatorImpl::~PrimaryAccountMutatorImpl() {}

bool PrimaryAccountMutatorImpl::SetPrimaryAccount(
    const std::string& account_id) {
  NOTIMPLEMENTED();
  return false;
}

void PrimaryAccountMutatorImpl::ClearPrimaryAccount(
    ClearAccountsAction action,
    signin_metrics::ProfileSignout source_metric,
    signin_metrics::SignoutDelete delete_metric) {
  NOTIMPLEMENTED();
}

bool PrimaryAccountMutatorImpl::IsSettingPrimaryAccountAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

void PrimaryAccountMutatorImpl::SetSettingPrimaryAccountAllowed(bool allowed) {
  NOTIMPLEMENTED();
}

bool PrimaryAccountMutatorImpl::IsClearingPrimaryAccountAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

void PrimaryAccountMutatorImpl::SetClearingPrimaryAccountAllowed(bool allowed) {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::SetAllowedPrimaryAccountPattern(
    const std::string& pattern) {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::
    LegacyStartSigninWithRefreshTokenForPrimaryAccount(
        const std::string& refresh_token,
        const std::string& gaia_id,
        const std::string& username,
        const std::string& password,
        base::RepeatingCallback<void(const std::string&)> callback) {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::LegacyCompletePendingPrimaryAccountSignin() {
  NOTIMPLEMENTED();
}

void PrimaryAccountMutatorImpl::LegacyMergeSigninCredentialIntoCookieJar() {
  NOTIMPLEMENTED();
}

bool PrimaryAccountMutatorImpl::LegacyIsPrimaryAccountAuthInProgress() const {
  NOTIMPLEMENTED();
  return false;
}

AccountInfo PrimaryAccountMutatorImpl::LegacyPrimaryAccountForAuthInProgress()
    const {
  NOTIMPLEMENTED();
  return AccountInfo{};
}

void PrimaryAccountMutatorImpl::LegacyCopyCredentialsFrom(
    const PrimaryAccountMutator& source) {
  NOTIMPLEMENTED();
}

}  // namespace identity
