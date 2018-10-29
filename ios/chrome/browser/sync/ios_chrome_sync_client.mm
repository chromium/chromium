// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_components_factory_impl.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/sync/history_model_worker.h"
#include "components/history/core/browser/sync/typed_url_sync_bridge.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_model_worker.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/search_engines/search_engine_data_type_controller.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sequenced_model_worker.h"
#include "components/sync/engine/ui_model_worker.h"
#include "components/sync/user_events/user_event_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/favicon_cache.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_sync_service_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "ios/chrome/browser/favicon/favicon_service_factory.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_deprecated_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/invalidation/ios_chrome_profile_invalidation_provider_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/session_sync_service_factory.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

syncer::ModelTypeSet GetDisabledTypesFromCommandLine() {
  std::string disabled_types_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDisableSyncTypes);

  return syncer::ModelTypeSetFromString(disabled_types_str);
}

}  // namespace

IOSChromeSyncClient::IOSChromeSyncClient(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  profile_web_data_service_ =
      ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
          browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)
          ? ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
                browser_state_, ServiceAccessType::IMPLICIT_ACCESS)
          : nullptr;
  db_thread_ = profile_web_data_service_
                   ? profile_web_data_service_->GetDBTaskRunner()
                   : nullptr;
  password_store_ = IOSChromePasswordStoreFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);

  // Component factory may already be set in tests.
  if (!GetSyncApiComponentFactory()) {
    component_factory_.reset(new browser_sync::ProfileSyncComponentsFactoryImpl(
        this, ::GetChannel(), ::GetVersionString(),
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET,
        prefs::kSavingBrowserHistoryDisabled,
        base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI}),
        db_thread_, profile_web_data_service_, account_web_data_service_,
        password_store_,
        ios::BookmarkSyncServiceFactory::GetForBrowserState(browser_state_)));
  }
}

IOSChromeSyncClient::~IOSChromeSyncClient() {}

syncer::SyncService* IOSChromeSyncClient::GetSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ProfileSyncServiceFactory::GetForBrowserState(browser_state_);
}

PrefService* IOSChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return browser_state_->GetPrefs();
}

base::FilePath IOSChromeSyncClient::GetLocalSyncBackendFolder() {
  return base::FilePath();
}

syncer::ModelTypeStoreService* IOSChromeSyncClient::GetModelTypeStoreService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ModelTypeStoreServiceFactory::GetForBrowserState(browser_state_);
}

bookmarks::BookmarkModel* IOSChromeSyncClient::GetBookmarkModel() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ios::BookmarkModelFactory::GetForBrowserState(browser_state_);
}

favicon::FaviconService* IOSChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ios::FaviconServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::IMPLICIT_ACCESS);
}

history::HistoryService* IOSChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

sync_sessions::SessionSyncService*
IOSChromeSyncClient::GetSessionSyncService() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return SessionSyncServiceFactory::GetForBrowserState(browser_state_);
}

bool IOSChromeSyncClient::HasPasswordStore() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return password_store_ != nullptr;
}

autofill::PersonalDataManager* IOSChromeSyncClient::GetPersonalDataManager() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return autofill::PersonalDataManagerFactory::GetForBrowserState(
      browser_state_);
}

base::Closure IOSChromeSyncClient::GetPasswordStateChangedCallback() {
  return base::Bind(
      &IOSChromePasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged,
      base::Unretained(browser_state_));
}

syncer::DataTypeController::TypeVector
IOSChromeSyncClient::CreateDataTypeControllers(
    syncer::LocalDeviceInfoProvider* local_device_info_provider) {
  // The iOS port does not have any platform-specific datatypes.
  return component_factory_->CreateCommonDataTypeControllers(
      GetDisabledTypesFromCommandLine(), local_device_info_provider);
}

BookmarkUndoService* IOSChromeSyncClient::GetBookmarkUndoServiceIfExists() {
  return ios::BookmarkUndoServiceFactory::GetForBrowserStateIfExists(
      browser_state_);
}

invalidation::InvalidationService*
IOSChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider;

  if (base::FeatureList::IsEnabled(invalidation::switches::kFCMInvalidations)) {
    provider = IOSChromeProfileInvalidationProviderFactory::GetForBrowserState(
        browser_state_);
  } else {
    provider = IOSChromeDeprecatedProfileInvalidationProviderFactory::
        GetForBrowserState(browser_state_);
  }
  if (provider)
    return provider->GetInvalidationService();
  return nullptr;
}

scoped_refptr<syncer::ExtensionsActivity>
IOSChromeSyncClient::GetExtensionsActivity() {
  return nullptr;
}

base::WeakPtr<syncer::SyncableService>
IOSChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::PREFERENCES:
      return browser_state_->GetSyncablePrefs()
          ->GetSyncableService(syncer::PREFERENCES)
          ->AsWeakPtr();
    case syncer::PRIORITY_PREFERENCES:
      return browser_state_->GetSyncablePrefs()
          ->GetSyncableService(syncer::PRIORITY_PREFERENCES)
          ->AsWeakPtr();
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA: {
      if (!profile_web_data_service_)
        return base::WeakPtr<syncer::SyncableService>();
      if (type == syncer::AUTOFILL_PROFILE) {
        return autofill::AutofillProfileSyncableService::FromWebDataService(
                   profile_web_data_service_.get())
            ->AsWeakPtr();
      } else if (type == syncer::AUTOFILL_WALLET_METADATA) {
        return autofill::AutofillWalletMetadataSyncableService::
            FromWebDataService(profile_web_data_service_.get())
                ->AsWeakPtr();
      }
      return autofill::AutofillWalletSyncableService::FromWebDataService(
                 profile_web_data_service_.get())
          ->AsWeakPtr();
    }
    case syncer::HISTORY_DELETE_DIRECTIVES: {
      history::HistoryService* history =
          ios::HistoryServiceFactory::GetForBrowserState(
              browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
      return history ? history->AsWeakPtr()
                     : base::WeakPtr<history::HistoryService>();
    }
    case syncer::FAVICON_IMAGES:
    case syncer::FAVICON_TRACKING: {
      sync_sessions::FaviconCache* favicons =
          SessionSyncServiceFactory::GetForBrowserState(browser_state_)
              ->GetFaviconCache();
      return favicons ? favicons->AsWeakPtr()
                      : base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::DEPRECATED_ARTICLES: {
      // DomDistillerService is used in iOS ReadingList. The distilled articles
      // are saved separately and must not be synced.
      // Add a not reached to avoid having ARTICLES sync be enabled silently.
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
    }
    case syncer::SESSIONS: {
      return SessionSyncServiceFactory::GetForBrowserState(browser_state_)
          ->GetSyncableService()
          ->AsWeakPtr();
    }
    case syncer::PASSWORDS: {
      return password_store_ ? password_store_->GetPasswordSyncableService()
                             : base::WeakPtr<syncer::SyncableService>();
    }
    default:
      NOTREACHED();
      return base::WeakPtr<syncer::SyncableService>();
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
IOSChromeSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  switch (type) {
    case syncer::DEVICE_INFO:
      return ProfileSyncServiceFactory::GetForBrowserState(browser_state_)
          ->GetDeviceInfoSyncControllerDelegate();
    case syncer::READING_LIST: {
      ReadingListModel* reading_list_model =
          ReadingListModelFactory::GetForBrowserState(browser_state_);
      return reading_list_model->GetModelTypeSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();
    }
    case syncer::USER_CONSENTS:
      return ConsentAuditorFactory::GetForBrowserState(browser_state_)
          ->GetControllerDelegate();
    case syncer::USER_EVENTS:
      return IOSUserEventServiceFactory::GetForBrowserState(browser_state_)
          ->GetSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();

    // We don't exercise this function for certain datatypes, because their
    // controllers get the delegate elsewhere.
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA:
    case syncer::BOOKMARKS:
    case syncer::SESSIONS:
    case syncer::TYPED_URLS:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();

    default:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }
}

scoped_refptr<syncer::ModelSafeWorker>
IOSChromeSyncClient::CreateModelWorkerForGroup(syncer::ModelSafeGroup group) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  switch (group) {
    case syncer::GROUP_DB:
      return new syncer::SequencedModelWorker(db_thread_, syncer::GROUP_DB);
    case syncer::GROUP_FILE:
      // Not supported on iOS.
      return nullptr;
    case syncer::GROUP_UI:
      return new syncer::UIModelWorker(
          base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI}));
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker();
    case syncer::GROUP_HISTORY: {
      history::HistoryService* history_service = GetHistoryService();
      if (!history_service)
        return nullptr;
      return new browser_sync::HistoryModelWorker(
          history_service->AsWeakPtr(),
          base::CreateSingleThreadTaskRunnerWithTraits({web::WebThread::UI}));
    }
    case syncer::GROUP_PASSWORD: {
      if (!password_store_)
        return nullptr;
      return new browser_sync::PasswordModelWorker(password_store_);
    }
    default:
      return nullptr;
  }
}

syncer::SyncApiComponentFactory*
IOSChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

void IOSChromeSyncClient::SetSyncApiComponentFactoryForTesting(
    std::unique_ptr<syncer::SyncApiComponentFactory> component_factory) {
  component_factory_ = std::move(component_factory);
}

// static
void IOSChromeSyncClient::GetDeviceInfoTrackers(
    std::vector<const syncer::DeviceInfoTracker*>* trackers) {
  DCHECK(trackers);
  std::vector<ios::ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (ios::ChromeBrowserState* browser_state : browser_state_list) {
    browser_sync::ProfileSyncService* profile_sync_service =
        ProfileSyncServiceFactory::GetForBrowserState(browser_state);
    if (profile_sync_service != nullptr) {
      const syncer::DeviceInfoTracker* tracker =
          profile_sync_service->GetDeviceInfoTracker();
      if (tracker != nullptr) {
        trackers->push_back(tracker);
      }
    }
  }
}
