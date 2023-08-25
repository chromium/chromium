// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_ACCOUNT_COOKIE_WAITER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_ACCOUNT_COOKIE_WAITER_H_

#import "base/functional/callback_forward.h"
#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "google_apis/gaia/core_account_id.h"

// Helper to wait for a Gaia cookie to be populated.
class AccountCookieWaiter : public signin::IdentityManager::Observer {
 public:
  enum class Result {
    kSuccess,
    kAuthError,
    kTimeout,
  };

  explicit AccountCookieWaiter(signin::IdentityManager* identity_manager);

  AccountCookieWaiter(const AccountCookieWaiter&) = delete;
  AccountCookieWaiter& operator=(const AccountCookieWaiter&) = delete;

  // If `Wait()` is ongoing, aborts without invoking the callback.
  ~AccountCookieWaiter() override;

  // Waits for `account_id` to be present in the cookie jar and invokes
  // `callback` with the outcome. Must not be called again before the previous
  // wait finishes, i.e. before `callback` is invoked.
  void Wait(CoreAccountId account_id,
            base::OnceCallback<void(Result)> callback);

  // signin::IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

 private:
  void OnTimeout();

  const raw_ptr<signin::IdentityManager> identity_manager_;

  base::OneShotTimer timer_;
  // Set on every Wait().
  CoreAccountId account_id_;
  // Set on every Wait().
  base::OnceCallback<void(Result)> callback_;
};

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_ACCOUNT_COOKIE_WAITER_H_
