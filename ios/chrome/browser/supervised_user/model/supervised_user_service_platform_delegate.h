// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_

#import "base/memory/raw_ref.h"
#import "components/supervised_user/core/browser/supervised_user_service.h"

class BrowserList;
class PrefService;

// Delegate handling iOS logic that is invoked from SupervisedUserService.
class SupervisedUserServicePlatformDelegate
    : public supervised_user::SupervisedUserService::PlatformDelegate {
 public:
  // `pref_service` and `browser_list` must outlive this object.
  SupervisedUserServicePlatformDelegate(PrefService& pref_service,
                                        BrowserList& browser_list);

  // supervised_user::SupervisedUserService::PlatformDelegate
  bool ShouldCloseIncognitoTabs() const override;
  // Closes all incognito tabs of all windows when supervised users sign in.
  void CloseIncognitoTabs() override;

 private:
  const raw_ref<PrefService> pref_service_;
  const raw_ref<BrowserList> browser_list_;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_H_
