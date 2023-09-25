// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/account_cookie_waiter.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/functional/callback.h"
#import "base/location.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/google_service_auth_error.h"

namespace {

// Sign-in time out duration.
constexpr base::TimeDelta kSigninTimeout = base::Seconds(10);

}  // namespace

AccountCookieWaiter::AccountCookieWaiter(
    signin::IdentityManager* identity_manager)
    : identity_manager_(identity_manager) {
  identity_manager_->AddObserver(this);
}

AccountCookieWaiter::~AccountCookieWaiter() {
  identity_manager_->RemoveObserver(this);
}

void AccountCookieWaiter::Wait(CoreAccountId account_id,
                               base::OnceCallback<void(Result)> callback) {
  CHECK(!timer_.IsRunning())
      << "Wait() musn't be called before previous call finished";

  account_id_ = account_id;
  callback_ = std::move(callback);

  // Unretained() is safe because `this` outlives `timer_`.
  timer_.Start(
      FROM_HERE, kSigninTimeout,
      base::BindOnce(&AccountCookieWaiter::OnTimeout, base::Unretained(this)));
}

void AccountCookieWaiter::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if (!timer_.IsRunning()) {
    // No waiting ongoing.
    return;
  }

  if (error.state() != GoogleServiceAuthError::State::NONE) {
    timer_.Stop();
    std::move(callback_).Run(Result::kAuthError);
  } else if (base::Contains(accounts_in_cookie_jar_info.signed_in_accounts,
                            account_id_, &gaia::ListedAccount::id)) {
    timer_.Stop();
    std::move(callback_).Run(Result::kSuccess);
  } else {
    // Keep waiting.
  }
}

void AccountCookieWaiter::OnTimeout() {
  std::move(callback_).Run(Result::kTimeout);
}
