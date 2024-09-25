// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "build/build_config.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/core/browser/signin_error_controller.h"
#import "components/signin/public/identity_manager/identity_manager.h"

// This class listens to various signin events and updates the signin-related
// fields of ProfileAttributesStorageIOS.
// TODO(crbug.com/361040177): Rename this class to SigninProfileInfoUpdater.
class SigninProfileInfoUpdater : public KeyedService,
                                 public SigninErrorController::Observer,
                                 public signin::IdentityManager::Observer {
 public:
  SigninProfileInfoUpdater(signin::IdentityManager* identity_manager,
                           SigninErrorController* signin_error_controller,
                           const std::string& profile_name);

  SigninProfileInfoUpdater(const SigninProfileInfoUpdater&) = delete;
  SigninProfileInfoUpdater& operator=(const SigninProfileInfoUpdater&) = delete;

  ~SigninProfileInfoUpdater() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // Updates the profile info on signin and signout events.
  void UpdateBrowserStateInfo();

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  raw_ptr<SigninErrorController> signin_error_controller_ = nullptr;
  const std::string profile_name_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  base::ScopedObservation<SigninErrorController,
                          SigninErrorController::Observer>
      signin_error_controller_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SIGNIN_PROFILE_INFO_UPDATER_H_
