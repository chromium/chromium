// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ref.h"
#import "base/memory/scoped_refptr.h"
#import "base/scoped_observation.h"
#import "base/task/sequenced_task_runner.h"
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
  void UpdateLoadedAccounts();
  void HandleMigrationIfNeeded();

  const raw_ref<SystemIdentityManager> system_identity_manager_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::ScopedObservation<SystemIdentityManager, SystemIdentityManagerObserver>
      system_identity_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_ACCOUNT_UPDATER_H_
