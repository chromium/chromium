// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_ACCOUNT_MANAGER_CHROMIUM_H_
#define REMOTING_IOS_APP_ACCOUNT_MANAGER_CHROMIUM_H_

#include "remoting/ios/app/account_manager.h"

namespace remoting {
namespace ios {

class AccountManagerChromium final : public AccountManager {
 public:
  AccountManagerChromium();
  ~AccountManagerChromium() override;

  AccountManagerChromium(const AccountManagerChromium&) = delete;
  AccountManagerChromium& operator=(const AccountManagerChromium&) = delete;

  // AccountManager overrides.
  UIViewController* CreateAccountParticleDiscViewController() override;
  void PresentSignInMenu() override;
};

}  // namespace ios
}  // namespace remoting

#endif  // REMOTING_IOS_APP_ACCOUNT_MANAGER_CHROMIUM_H_
