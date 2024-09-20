// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/tailored_security/chrome_tailored_security_service.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

ChromeTailoredSecurityService::ChromeTailoredSecurityService(
    ProfileIOS* profile,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service)
    : TailoredSecurityService(identity_manager,
                              sync_service,
                              profile->GetPrefs()),
      profile_(profile) {
  base::WeakPtr<ChromeTailoredSecurityService> weak_ptr =
      weak_ptr_factory_.GetWeakPtr();
  application_backgrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidEnterBackgroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (weak_ptr.get()) {
                  weak_ptr->AppDidEnterBackground();
                }
              }];

  application_foregrounding_observer_ = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationWillEnterForegroundNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (weak_ptr.get()) {
                  weak_ptr->AppWillEnterForeground();
                }
              }];
}

ChromeTailoredSecurityService::~ChromeTailoredSecurityService() {
  DCHECK(application_foregrounding_observer_);
  DCHECK(application_backgrounding_observer_);
  [[NSNotificationCenter defaultCenter]
      removeObserver:application_backgrounding_observer_];
  application_backgrounding_observer_ = nil;

  [[NSNotificationCenter defaultCenter]
      removeObserver:application_foregrounding_observer_];
  application_foregrounding_observer_ = nil;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeTailoredSecurityService::GetURLLoaderFactory() {
  return profile_->GetSharedURLLoaderFactory();
}

void ChromeTailoredSecurityService::AppDidEnterBackground() {
  this->SetCanQuery(false);
}

void ChromeTailoredSecurityService::AppWillEnterForeground() {
  this->SetCanQuery(true);
}

}  // namespace safe_browsing
