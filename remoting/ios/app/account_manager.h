// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_ACCOUNT_MANAGER_H_
#define REMOTING_IOS_APP_ACCOUNT_MANAGER_H_

#include <memory>

#import <UIKit/UIKit.h>

namespace remoting {
namespace ios {

// An interface that provides UI components to manage the user's account. The
// implementation may come from some internal libraries.
//
// This interface does not deal with callbacks of user sign-in, sign-out, or
// account switching. For these events you can listen to the kUserDidUpdate
// event defined in remoting_service.h.
class AccountManager {
 public:
  AccountManager();
  virtual ~AccountManager();

  // Sets the AccountManager singleton. Can only be called once.
  static void SetInstance(std::unique_ptr<AccountManager> account_manager);

  // Gets the AccountManager instance.
  static AccountManager* GetInstance();

  // Creates a view controller that renders an account particle disc, a little
  // circular button that shows the user's avatar image and pops up the account
  // management menu.
  virtual UIViewController* CreateAccountParticleDiscViewController() = 0;

  // Presents a menu that allows the user to choose an account to sign in or add
  // a new account. This is usually used when the app is first launched or the
  // user has previously signed out.
  virtual void PresentSignInMenu() = 0;
};

}  // namespace ios
}  // namespace remoting

#endif  // REMOTING_IOS_APP_ACCOUNT_MANAGER_H_