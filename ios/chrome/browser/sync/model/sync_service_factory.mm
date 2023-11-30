// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/history/core/browser/features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/network_time/network_time_tracker.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/supervised_user/core/common/buildflags.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_updater_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/about_signin_internals_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

namespace {

std::unique_ptr<KeyedService> BuildSyncService(web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  DCHECK(!browser_state->IsOffTheRecord());

  // Always create the GCMProfileService instance such that we can listen to
  // the profile notifications and purge the GCM store when the profile is
  // being signed out.
  IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state);

  // TODO(crbug.com/171406): Change AboutSigninInternalsFactory to load on
  // startup once bug has been fixed.
  ios::AboutSigninInternalsFactory::GetForBrowserState(browser_state);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  init_params.sync_client =
      std::make_unique<IOSChromeSyncClient>(browser_state);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      GetApplicationContext()->GetNetworkConnectionTracker();
  init_params.channel = ::GetChannel();
  init_params.debug_identifier = browser_state->GetDebugName();

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize();

  // TODO(crbug.com/1400663): Remove the workaround below once
  // PrivacySandboxSettingsFactory correctly declares its KeyedServices
  // dependencies.
  if (history::IsSyncSegmentsDataEnabled()) {
    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForBrowserStateIfExists(
            browser_state, ServiceAccessType::EXPLICIT_ACCESS);

    syncer::DeviceInfoSyncService* device_info_sync_service =
        DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);

    if (history_service && device_info_sync_service) {
      PrefService* local_state = GetApplicationContext()->GetLocalState();
      CHECK(local_state);

      int display_count = local_state->GetInteger(
          prefs::kIosSyncSegmentsNewTabPageDisplayCount);

      int display_limit = history::kMaxNumNewTabPageDisplays.Get();

      history_service->SetCanAddForeignVisitsToSegmentsOnBackend(display_count <
                                                                 display_limit);

      history_service->SetDeviceInfoServices(
          device_info_sync_service->GetDeviceInfoTracker(),
          device_info_sync_service->GetLocalDeviceInfoProvider());
    }
  }

  password_manager::PasswordReceiverService* password_receiver_service =
      IOSChromePasswordReceiverServiceFactory::GetForBrowserState(
          browser_state);
  if (password_receiver_service) {
    password_receiver_service->OnSyncServiceInitialized(sync_service.get());
  }

  // Allow sync_preferences/ components to use SyncService.
  sync_preferences::PrefServiceSyncable* pref_service =
      browser_state->GetSyncablePrefs();
  pref_service->OnSyncServiceInitialized(sync_service.get());

  SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state)
      ->OnSyncServiceInitialized(sync_service.get());

  return sync_service;
}

}  // namespace

// static
SyncServiceFactory* SyncServiceFactory::GetInstance() {
  static base::NoDestructor<SyncServiceFactory> instance;
  return instance.get();
}

// static
SyncServiceFactory::TestingFactory SyncServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSyncService);
}

// static
syncer::SyncService* SyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
syncer::SyncService* SyncServiceFactory::GetForBrowserStateIfExists(
    ChromeBrowserState* browser_state) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
syncer::SyncServiceImpl*
SyncServiceFactory::GetAsSyncServiceImplForBrowserStateForTesting(
    ChromeBrowserState* browser_state) {
  return static_cast<syncer::SyncServiceImpl*>(
      GetForBrowserState(browser_state));
}

SyncServiceFactory::SyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The SyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  // Sync needs this service to still be present when the sync engine is
  // disabled, so that preferences can be cleared.
  DependsOn(GoogleGroupsUpdaterServiceFactory::GetInstance());
  DependsOn(ios::AboutSigninInternalsFactory::GetInstance());
  DependsOn(ios::AccountBookmarkModelFactory::GetInstance());
  DependsOn(ios::AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(ios::WebDataServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromePasswordReceiverServiceFactory::GetInstance());
  DependsOn(IOSChromePasswordSenderServiceFactory::GetInstance());
  DependsOn(IOSChromeProfilePasswordStoreFactory::GetInstance());
  DependsOn(IOSChromeAccountPasswordStoreFactory::GetInstance());
  DependsOn(IOSTrustedVaultServiceFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
}

SyncServiceFactory::~SyncServiceFactory() {}

std::unique_ptr<KeyedService> SyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSyncService(context);
}
