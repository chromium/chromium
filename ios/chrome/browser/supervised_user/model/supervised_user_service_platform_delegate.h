// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_

#include "components/supervised_user/core/browser/supervised_user_service.h"

class ChromeBrowserState;

// Delegate handling iOS logic that is invoked from SupervisedUserService.
class SupervisedUserServicePlatformDelegate
    : public supervised_user::SupervisedUserService::PlatformDelegate {
 public:
  explicit SupervisedUserServicePlatformDelegate(
      ChromeBrowserState* browser_state);

  // Closes all incognito tabs of all windows when supervised users sign in.
  void CloseIncognitoTabs() override;

 private:
  ChromeBrowserState* browser_state_;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
