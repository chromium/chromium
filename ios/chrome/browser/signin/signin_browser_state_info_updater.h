// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_
#define IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// This class listens to various signin events and updates the signin-related
// fields of BrowserStateInfoCache.
class SigninBrowserStateInfoUpdater : public KeyedService,
                                      public SigninErrorController::Observer,
                                      public signin::IdentityManager::Observer {
 public:
  SigninBrowserStateInfoUpdater(signin::IdentityManager* identity_manager,
                                SigninErrorController* signin_error_controller,
                                const base::FilePath& browser_state_path);

  ~SigninBrowserStateInfoUpdater() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // Updates the browser state info on signin and signout events.
  void UpdateBrowserStateInfo();

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

  signin::IdentityManager* identity_manager_ = nullptr;
  SigninErrorController* signin_error_controller_ = nullptr;
  const base::FilePath browser_state_path_;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      identity_manager_observer_{this};
  ScopedObserver<SigninErrorController, SigninErrorController::Observer>
      signin_error_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninBrowserStateInfoUpdater);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_STATE_INFO_UPDATER_H_
