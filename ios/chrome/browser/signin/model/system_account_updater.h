// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer.h"

@protocol RefreshAccessTokenError;
@protocol SystemIdentity;

// Helper class that handles system account updates.
class SystemAccountUpdater : public SystemIdentityManagerObserver {
 public:
  explicit SystemAccountUpdater(SystemIdentityManager* system_identity_manager);
  ~SystemAccountUpdater() override;

  // SystemIdentityManagerObserver implementation.
  void OnIdentityListChanged() final;
  void OnIdentityUpdated(id<SystemIdentity> identity) final;

 private:
  SystemIdentityManager::IteratorResult IdentitiesOnDevice(
      NSMutableDictionary* accounts,
      NSMutableDictionary* avatars,
      id<SystemIdentity> identity);
  void UpdateLoadedAccounts();
  void HandleMigrationIfNeeded();

  raw_ptr<SystemIdentityManager> system_identity_manager_;

  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};

  base::WeakPtrFactory<SystemAccountUpdater> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_
