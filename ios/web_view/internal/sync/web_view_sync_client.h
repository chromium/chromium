// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/driver/sync_client.h"

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace syncer {
class SyncApiComponentFactory;
class SyncService;
}  // namespace syncer

namespace ios_web_view {

class WebViewBrowserState;

class WebViewSyncClient : public syncer::SyncClient {
 public:
  WebViewSyncClient(WebViewBrowserState* browser_state);
  ~WebViewSyncClient() override;

  // SyncClient implementation.
  syncer::SyncService* GetSyncService() override;
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  bool HasPasswordStore() override;
  base::RepeatingClosure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::LocalDeviceInfoProvider* local_device_info_provider) override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  invalidation::InvalidationService* GetInvalidationService() override;
  BookmarkUndoService* GetBookmarkUndoServiceIfExists() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;

 private:
  WebViewBrowserState* browser_state_ = nullptr;
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  std::unique_ptr<syncer::SyncApiComponentFactory> component_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSyncClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
