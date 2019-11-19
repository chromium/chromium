// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/profile_sync_components_factory_impl.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/sync/password_model_worker.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sequenced_model_worker.h"
#include "components/sync/engine/ui_model_worker.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_string.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web_view/internal/passwords/web_view_password_store_factory.h"
#include "ios/web_view/internal/pref_names.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_profile_invalidation_provider_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

namespace {
syncer::ModelTypeSet GetDisabledTypes() {
  syncer::ModelTypeSet disabled_types = syncer::UserTypes();
  disabled_types.Remove(syncer::AUTOFILL_WALLET_DATA);
  disabled_types.Remove(syncer::AUTOFILL_WALLET_METADATA);
  disabled_types.Remove(syncer::AUTOFILL_PROFILE);
  disabled_types.Remove(syncer::PASSWORDS);
  return disabled_types;
}
}  // namespace

WebViewSyncClient::WebViewSyncClient(WebViewBrowserState* browser_state)
    : browser_state_(browser_state) {
  profile_web_data_service_ =
      WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)
          ? WebViewWebDataServiceWrapperFactory::GetAutofillWebDataForAccount(
                browser_state_, ServiceAccessType::IMPLICIT_ACCESS)
          : nullptr;

  db_thread_ = profile_web_data_service_
                   ? profile_web_data_service_->GetDBTaskRunner()
                   : nullptr;

  password_store_ = WebViewPasswordStoreFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  component_factory_ =
      std::make_unique<browser_sync::ProfileSyncComponentsFactoryImpl>(
          this, version_info::Channel::STABLE,
          prefs::kSavingBrowserHistoryDisabled,
          base::CreateSingleThreadTaskRunner({web::WebThread::UI}), db_thread_,
          profile_web_data_service_, account_web_data_service_, password_store_,
          /*account_password_store=*/nullptr,
          /*bookmark_sync_service=*/nullptr);
}

WebViewSyncClient::~WebViewSyncClient() {}

PrefService* WebViewSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state_->GetPrefs();
}

base::FilePath WebViewSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::ModelTypeStoreService* WebViewSyncClient::GetModelTypeStoreService() {
  return WebViewModelTypeStoreServiceFactory::GetForBrowserState(
      browser_state_);
}

syncer::DeviceInfoSyncService* WebViewSyncClient::GetDeviceInfoSyncService() {
  return WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(
      browser_state_);
}

bookmarks::BookmarkModel* WebViewSyncClient::GetBookmarkModel() {
  return nullptr;
}

favicon::FaviconService* WebViewSyncClient::GetFaviconService() {
  return nullptr;
}

history::HistoryService* WebViewSyncClient::GetHistoryService() {
  return nullptr;
}

sync_sessions::SessionSyncService* WebViewSyncClient::GetSessionSyncService() {
  return nullptr;
}

send_tab_to_self::SendTabToSelfSyncService*
WebViewSyncClient::GetSendTabToSelfSyncService() {
  return nullptr;
}

base::RepeatingClosure WebViewSyncClient::GetPasswordStateChangedCallback() {
  return base::BindRepeating(
      &WebViewPasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged,
      base::Unretained(browser_state_));
}

syncer::DataTypeController::TypeVector
WebViewSyncClient::CreateDataTypeControllers(
    syncer::SyncService* sync_service) {
  // //ios/web_view clients are supposed to record their own consents.
  syncer::DataTypeController::TypeVector type_vector =
      component_factory_->CreateCommonDataTypeControllers(GetDisabledTypes(),
                                                          sync_service);
  type_vector.erase(std::remove_if(type_vector.begin(), type_vector.end(),
                                   [](const auto& data_type_controller) {
                                     return data_type_controller->type() ==
                                            syncer::USER_CONSENTS;
                                   }));
  return type_vector;
}

BookmarkUndoService* WebViewSyncClient::GetBookmarkUndoService() {
  return nullptr;
}

invalidation::InvalidationService* WebViewSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      WebViewProfileInvalidationProviderFactory::GetForBrowserState(
          browser_state_);
  if (provider) {
    return provider->GetInvalidationService();
  }
  return nullptr;
}

syncer::TrustedVaultClient* WebViewSyncClient::GetTrustedVaultClient() {
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
WebViewSyncClient::GetExtensionsActivity() {
  return nullptr;
}

base::WeakPtr<syncer::SyncableService>
WebViewSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::PASSWORDS:
      return password_store_ ? password_store_->GetPasswordSyncableService()
                             : base::WeakPtr<syncer::SyncableService>();
    default:
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
WebViewSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  // Even though USER_CONSENTS are disabled, the component factory will create
  // its controller and ask for a delegate; this client later removes the
  // controller. No other data type should ask for its delegate.
  // TODO(crbug.com/873790): Figure out if USER_CONSENTS need to be enabled or
  // find a better way how to disable it.
  DCHECK_EQ(type, syncer::USER_CONSENTS);
  return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
}

scoped_refptr<syncer::ModelSafeWorker>
WebViewSyncClient::CreateModelWorkerForGroup(syncer::ModelSafeGroup group) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  switch (group) {
    case syncer::GROUP_UI:
      return new syncer::UIModelWorker(
          base::CreateSingleThreadTaskRunner({web::WebThread::UI}));
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker();
    case syncer::GROUP_PASSWORD:
      if (!password_store_)
        return nullptr;
      return new browser_sync::PasswordModelWorker(password_store_);
    default:
      return nullptr;
  }
}

syncer::SyncApiComponentFactory*
WebViewSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider* WebViewSyncClient::GetPreferenceProvider() {
  return nullptr;
}

}  // namespace ios_web_view
