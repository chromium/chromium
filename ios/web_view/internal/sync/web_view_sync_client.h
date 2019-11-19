// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/browser_sync/browser_sync_client.h"

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace browser_sync {
class ProfileSyncComponentsFactoryImpl;
}  // namespace browser_sync

namespace password_manager {
class PasswordStore;
}  // namespace password_manager

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ios_web_view {

class WebViewBrowserState;

class WebViewSyncClient : public browser_sync::BrowserSyncClient {
 public:
  explicit WebViewSyncClient(WebViewBrowserState* browser_state);
  ~WebViewSyncClient() override;

  // BrowserSyncClient implementation.
  PrefService* GetPrefService() override;
  base::FilePath GetLocalSyncBackendFolder() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  syncer::DeviceInfoSyncService* GetDeviceInfoSyncService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;
  favicon::FaviconService* GetFaviconService() override;
  history::HistoryService* GetHistoryService() override;
  send_tab_to_self::SendTabToSelfSyncService* GetSendTabToSelfSyncService()
      override;
  sync_sessions::SessionSyncService* GetSessionSyncService() override;
  base::RepeatingClosure GetPasswordStateChangedCallback() override;
  syncer::DataTypeController::TypeVector CreateDataTypeControllers(
      syncer::SyncService* sync_service) override;
  invalidation::InvalidationService* GetInvalidationService() override;
  syncer::TrustedVaultClient* GetTrustedVaultClient() override;
  BookmarkUndoService* GetBookmarkUndoService() override;
  scoped_refptr<syncer::ExtensionsActivity> GetExtensionsActivity() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateForModelType(syncer::ModelType type) override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  syncer::SyncApiComponentFactory* GetSyncApiComponentFactory() override;
  syncer::SyncTypePreferenceProvider* GetPreferenceProvider() override;

 private:
  WebViewBrowserState* browser_state_ = nullptr;
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  // TODO(crbug.com/915154): Revert to SyncApiComponentFactory once common
  // controller creation is moved elsewhere.
  std::unique_ptr<browser_sync::ProfileSyncComponentsFactoryImpl>
      component_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> db_thread_;

  DISALLOW_COPY_AND_ASSIGN(WebViewSyncClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_SYNC_CLIENT_H_
