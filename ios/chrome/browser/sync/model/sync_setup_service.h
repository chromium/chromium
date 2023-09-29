// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SETUP_SERVICE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SETUP_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_user_settings.h"

namespace syncer {
class SyncService;
class SyncSetupInProgressHandle;
}  // namespace syncer

// Class that allows configuring sync. It handles enabling and disabling it, as
// well as choosing datatypes. Most actions are delayed until a commit is done,
// to allow the complex sync setup flow on iOS.
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// DEPRECATED! Please do not add new usages. Use SyncService directly instead!
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
class SyncSetupService : public KeyedService {
 public:
  explicit SyncSetupService(syncer::SyncService* sync_service);

  SyncSetupService(const SyncSetupService&) = delete;
  SyncSetupService& operator=(const SyncSetupService&) = delete;

  ~SyncSetupService() override;

  // Pauses sync allowing the user to configure what data to sync before
  // actually starting to sync data with the server.
  // TODO(crbug.com/1438800): Rename to PrepareForSyncSetup().
  virtual void PrepareForFirstSyncSetup();

  // Sets the first setup complete flag. This method doesn't commit sync
  // changes. PrepareForFirstSyncSetup() needs to be called before. This flag is
  // not set if the user didn't turn on sync.
  // This method should only be used with UnifiedConsent flag.
  virtual void SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource source);

  // Commits all the pending configuration changes to Sync.
  void CommitSyncChanges();

  // Returns true if there are uncommitted sync changes.
  bool HasUncommittedChanges();

 private:
  syncer::SyncService* const sync_service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_SETUP_SERVICE_H_
