// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_service_factory.h"

#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/browser_sync/common_controller_builder.h"
#import "components/history/core/browser/features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/network_time/network_time_tracker.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/sharing/password_receiver_service.h"
#import "components/plus_addresses/webdata/plus_address_webdata_service.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/features.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "components/sync_device_info/device_info.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/variations/service/google_groups_manager.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_sync_service_factory.h"
#import "ios/chrome/browser/consent_auditor/model/consent_auditor_factory.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/metrics/model/google_groups_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_receiver_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"
#import "ios/chrome/browser/signin/model/about_signin_internals_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_settings_service_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/ios_chrome_sync_client.h"
#import "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/browser/trusted_vault/model/ios_trusted_vault_service_factory.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace {

syncer::DataTypeController::TypeVector CreateControllers(
    ProfileIOS* profile,
    syncer::SyncService* sync_service) {
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service =
      ios::WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS);

  browser_sync::CommonControllerBuilder builder;
  builder.SetAutofillWebDataService(
      web::GetUIThreadTaskRunner({}), profile_web_data_service,
      ios::WebDataServiceFactory::GetAutofillWebDataForAccount(
          profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetBookmarkModel(ios::BookmarkModelFactory::GetForProfile(profile));
  builder.SetBookmarkSyncService(
      ios::LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile),
      ios::AccountBookmarkSyncServiceFactory::GetForProfile(profile));
  builder.SetConsentAuditor(ConsentAuditorFactory::GetForProfile(profile));
  builder.SetDataSharingService(
      data_sharing::DataSharingServiceFactory::GetForProfile(profile));
  builder.SetDeviceInfoSyncService(
      DeviceInfoSyncServiceFactory::GetForProfile(profile));
  builder.SetDualReadingListModel(
      ReadingListModelFactory::GetAsDualReadingListModelForProfile(profile));
  builder.SetFaviconService(ios::FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetGoogleGroupsManager(
      GoogleGroupsManagerFactory::GetForProfile(profile));
  builder.SetHistoryService(ios::HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));
  builder.SetIdentityManager(IdentityManagerFactory::GetForProfile(profile));
  builder.SetDataTypeStoreService(
      DataTypeStoreServiceFactory::GetForProfile(profile));
  builder.SetPasskeyModel(
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? IOSPasskeyModelFactory::GetForProfile(profile)
          : nullptr);
  builder.SetPasswordReceiverService(
      IOSChromePasswordReceiverServiceFactory::GetForProfile(profile));
  builder.SetPasswordSenderService(
      IOSChromePasswordSenderServiceFactory::GetForProfile(profile));
  builder.SetPasswordStore(IOSChromeProfilePasswordStoreFactory::GetForProfile(
                               profile, ServiceAccessType::IMPLICIT_ACCESS),
                           IOSChromeAccountPasswordStoreFactory::GetForProfile(
                               profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPlusAddressServices(
      PlusAddressSettingServiceFactory::GetForProfile(profile),
      ios::WebDataServiceFactory::GetPlusAddressWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPowerBookmarkService(
      PowerBookmarkServiceFactory::GetForProfile(profile));
  builder.SetPrefService(profile->GetPrefs());
  builder.SetPrefServiceSyncable(profile->GetSyncablePrefs());
  // TODO(crbug.com/330201909) implement for iOS.
  builder.SetProductSpecificationsService(nullptr);
  builder.SetSendTabToSelfSyncService(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile));
  builder.SetSessionSyncService(
      SessionSyncServiceFactory::GetForProfile(profile));
  builder.SetSharingMessageBridge(
      base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)
          ? IOSSharingMessageBridgeFactory::GetForProfile(profile)
          : nullptr);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetSupervisedUserSettingsService(
      SupervisedUserSettingsServiceFactory::GetForProfile(profile));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetTabGroupSyncService(
      IsTabGroupSyncEnabled()
          ? tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)
          : nullptr);
  builder.SetTemplateURLService(nullptr);
  builder.SetUserEventService(
      IOSUserEventServiceFactory::GetForProfile(profile));

  syncer::DataTypeController::TypeVector controllers = builder.Build(
      /*disabled_types=*/{}, sync_service, ::GetChannel());

  return controllers;
}

// The maximum number of New Tab Page displays to show with synced segments
// data.
constexpr int kMaxSyncedNewTabPageDisplays = 5;

std::unique_ptr<KeyedService> BuildSyncService(web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  DCHECK(!profile->IsOffTheRecord());

  // Always create the GCMProfileService instance such that we can listen to
  // the profile notifications and purge the GCM store when the profile is
  // being signed out.
  IOSChromeGCMProfileServiceFactory::GetForProfile(profile);

  // TODO(crbug.com/40299450): Change AboutSigninInternalsFactory to load on
  // startup once bug has been fixed.
  ios::AboutSigninInternalsFactory::GetForProfile(profile);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.sync_client = std::make_unique<IOSChromeSyncClient>(profile);
  init_params.url_loader_factory = profile->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      GetApplicationContext()->GetNetworkConnectionTracker();
  init_params.channel = ::GetChannel();
  init_params.debug_identifier = profile->GetProfileName();

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize(CreateControllers(profile, sync_service.get()));

  // TODO(crbug.com/40250371): Remove the workaround below once
  // PrivacySandboxSettingsFactory correctly declares its KeyedServices
  // dependencies.
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForProfileIfExists(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);

  if (history_service && device_info_sync_service) {
    PrefService* pref_service = profile->GetPrefs();

    const int display_count =
        pref_service->GetInteger(prefs::kIosSyncSegmentsNewTabPageDisplayCount);

    history_service->SetCanAddForeignVisitsToSegmentsOnBackend(
        display_count < kMaxSyncedNewTabPageDisplays);

    history_service->SetDeviceInfoServices(
        device_info_sync_service->GetDeviceInfoTracker(),
        device_info_sync_service->GetLocalDeviceInfoProvider());
  }

  password_manager::PasswordReceiverService* password_receiver_service =
      IOSChromePasswordReceiverServiceFactory::GetForProfile(profile);
  if (password_receiver_service) {
    password_receiver_service->OnSyncServiceInitialized(sync_service.get());
  }

  // Allow sync_preferences/ components to use SyncService.
  sync_preferences::PrefServiceSyncable* pref_service =
      profile->GetSyncablePrefs();
  pref_service->OnSyncServiceInitialized(sync_service.get());

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->OnSyncServiceInitialized(sync_service.get());

  if (GoogleGroupsManager* groups_updater_service =
          GoogleGroupsManagerFactory::GetForProfile(profile)) {
    groups_updater_service->OnSyncServiceInitialized(sync_service.get());
  }

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
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
syncer::SyncService* SyncServiceFactory::GetForProfile(ProfileIOS* profile) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
syncer::SyncService* SyncServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
syncer::SyncServiceImpl*
SyncServiceFactory::GetAsSyncServiceImplForBrowserStateForTesting(
    ProfileIOS* profile) {
  return static_cast<syncer::SyncServiceImpl*>(GetForProfile(profile));
}

SyncServiceFactory::SyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The SyncServiceImpl depends on various KeyedServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order. Note that some of the dependencies are listed here but
  // actually plumbed in IOSChromeSyncClient, which this factory constructs.
  DependsOn(ChromeAccountManagerServiceFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(ios::AboutSigninInternalsFactory::GetInstance());
  DependsOn(ios::AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(ios::BookmarkModelFactory::GetInstance());
  DependsOn(ios::BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ios::LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
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
  if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
    DependsOn(IOSPasskeyModelFactory::GetInstance());
  }
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    DependsOn(IOSSharingMessageBridgeFactory::GetInstance());
  }
  DependsOn(IOSTrustedVaultServiceFactory::GetInstance());
  DependsOn(IOSUserEventServiceFactory::GetInstance());
  DependsOn(PlusAddressSettingServiceFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(ReadingListModelFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
}

SyncServiceFactory::~SyncServiceFactory() {}

std::unique_ptr<KeyedService> SyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSyncService(context);
}
