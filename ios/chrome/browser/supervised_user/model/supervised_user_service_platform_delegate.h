// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_

#import "base/memory/raw_ptr.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Delegate handling iOS logic that is invoked from SupervisedUserService.
class SupervisedUserServicePlatformDelegate
    : public supervised_user::SupervisedUserService::PlatformDelegate {
 public:
  explicit SupervisedUserServicePlatformDelegate(ProfileIOS* profile);

  // supervised_user::SupervisedUserService::PlatformDelegate
  std::string GetCountryCode() const override;
  version_info::Channel GetChannel() const override;

  // Closes all incognito tabs of all windows when supervised users sign in.
  void CloseIncognitoTabs() override;

 private:
  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
