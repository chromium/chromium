// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"

#import <optional>
#import <utility>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/singleton.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/send_tab_to_self/features.h"
#import "components/signin/public/base/device_id_helper.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/invalidations/sync_invalidations_service.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/sync_device_info/device_info_sync_client.h"
#import "components/sync_device_info/device_info_sync_service_impl.h"
#import "components/sync_device_info/local_device_info_provider_impl.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_invalidations_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace {

class DeviceInfoSyncClient : public syncer::DeviceInfoSyncClient {
 public:
  DeviceInfoSyncClient(
      PrefService* prefs,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      signin::IdentityManager* identity_manager)
      : prefs_(prefs),
        sync_invalidations_service_(sync_invalidations_service),
        identity_manager_(identity_manager) {}
  ~DeviceInfoSyncClient() override = default;

  // syncer::DeviceInfoSyncClient:
  std::string GetSigninScopedDeviceId() const override {
    return signin::GetSigninScopedDeviceId(prefs_);
  }

  // syncer::DeviceInfoSyncClient:
  bool GetSendTabToSelfReceivingEnabled() const override {
    // Always true starting with M101, see crbug.com/1299833. Older clients and
    // clients from other embedders might still return false.
    return true;
  }

  // syncer::DeviceInfoSyncClient:
  sync_pb::SyncEnums_SendTabReceivingType GetSendTabToSelfReceivingType()
      const override {
    std::string gaia_id =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia;
    bool send_tab_notifications_enabled = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            PushNotificationClientId::kSendTab, gaia_id);
    if (base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfIOSPushNotifications) &&
        send_tab_notifications_enabled) {
      return sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_AND_PUSH_NOTIFICATION;
    }
    return sync_pb::
        SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED;
  }

  // syncer::DeviceInfoSyncClient:
  std::optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override {
    if (!identity_manager_ ||
        !base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
      return std::nullopt;
    }
    std::string gaia_id =
        identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
            .gaia;
    std::string representative_target_id =
        GetApplicationContext()
            ->GetPushNotificationService()
            ->GetRepresentativeTargetIdForGaiaId(
                base::SysUTF8ToNSString(gaia_id));
    // Sharing info is not implemented on iOS, so empty structs are passed in.
    // TODO(crbug.com/352370268): Use SharingSyncPreference to hold SharingInfo.
    return syncer::DeviceInfo::SharingInfo(
        syncer::DeviceInfo::SharingTargetInfo(),
        syncer::DeviceInfo::SharingTargetInfo(), representative_target_id,
        std::set<sync_pb::SharingSpecificFields_EnabledFeatures>());
  }

  // syncer::DeviceInfoSyncClient:
  syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
  GetPhoneAsASecurityKeyInfo() const override {
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  // syncer::DeviceInfoSyncClient:
  std::optional<std::string> GetFCMRegistrationToken() const override {
    if (sync_invalidations_service_) {
      return sync_invalidations_service_->GetFCMRegistrationToken();
    }
    // If the service is not enabled, then the registration token must be empty,
    // not unknown (std::nullopt). This is needed to reset previous token if
    // the invalidations have been turned off.
    return std::string();
  }

  // syncer::DeviceInfoSyncClient:
  std::optional<syncer::DataTypeSet> GetInterestedDataTypes() const override {
    if (sync_invalidations_service_) {
      return sync_invalidations_service_->GetInterestedDataTypes();
    }
    // If the service is not enabled, then the list of types must be empty, not
    // unknown (std::nullopt). This is needed to reset previous types if the
    // invalidations have been turned off.
    return syncer::DataTypeSet();
  }

  // syncer::DeviceInfoSyncClient:
  // Returns false since we only care about Chrome OS devices
  bool IsUmaEnabledOnCrOSDevice() const override { return false; }

 private:
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace

// static
syncer::DeviceInfoSyncService* DeviceInfoSyncServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
DeviceInfoSyncServiceFactory* DeviceInfoSyncServiceFactory::GetInstance() {
  return base::Singleton<DeviceInfoSyncServiceFactory>::get();
}

// static
void DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers(
    std::vector<const syncer::DeviceInfoTracker*>* trackers) {
  DCHECK(trackers);
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    syncer::DeviceInfoSyncService* service =
        DeviceInfoSyncServiceFactory::GetForProfile(profile);
    if (service != nullptr) {
      const syncer::DeviceInfoTracker* tracker =
          service->GetDeviceInfoTracker();
      if (tracker != nullptr) {
        trackers->push_back(tracker);
      }
    }
  }
}

DeviceInfoSyncServiceFactory::DeviceInfoSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DeviceInfoSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DeviceInfoSyncServiceFactory::~DeviceInfoSyncServiceFactory() {}

std::unique_ptr<KeyedService>
DeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  syncer::SyncInvalidationsService* const sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForProfile(profile);
  signin::IdentityManager* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  auto device_info_sync_client = std::make_unique<DeviceInfoSyncClient>(
      profile->GetPrefs(), sync_invalidations_service, identity_manager);
  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          ::GetChannel(), ::GetVersionString(), device_info_sync_client.get());
  auto device_prefs = std::make_unique<syncer::DeviceInfoPrefs>(
      profile->GetPrefs(), base::DefaultClock::GetInstance());

  return std::make_unique<syncer::DeviceInfoSyncServiceImpl>(
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client), sync_invalidations_service);
}
