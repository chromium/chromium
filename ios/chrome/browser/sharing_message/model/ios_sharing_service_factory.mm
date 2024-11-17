// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing_message/model/ios_sharing_service_factory.h"

#import <memory>

#import "base/feature_list.h"
#import "base/no_destructor.h"
#import "build/build_config.h"
#import "components/gcm_driver/crypto/gcm_encryption_provider.h"
#import "components/gcm_driver/gcm_driver.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/sharing_message/ios_push/sharing_ios_push_sender.h"
#import "components/sharing_message/sharing_constants.h"
#import "components/sharing_message/sharing_device_registration.h"
#import "components/sharing_message/sharing_device_source_sync.h"
#import "components/sharing_message/sharing_fcm_handler.h"
#import "components/sharing_message/sharing_fcm_sender.h"
#import "components/sharing_message/sharing_message_sender.h"
#import "components/sharing_message/sharing_service.h"
#import "components/sharing_message/sharing_sync_preference.h"
#import "components/sharing_message/vapid_key_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/gcm/model/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_device_registration_impl.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_handler_registry_impl.h"
#import "ios/chrome/browser/sharing_message/model/ios_sharing_message_bridge_factory.h"
#import "ios/chrome/browser/sync/model/device_info_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {
// Removes old encryption info with empty authorized_entity to avoid DCHECK.
// See http://crbug/987591
void CleanEncryptionInfoWithoutAuthorizedEntity(gcm::GCMDriver* gcm_driver) {
  gcm::GCMEncryptionProvider* encryption_provider =
      gcm_driver->GetEncryptionProviderInternal();
  if (!encryption_provider) {
    return;
  }

  encryption_provider->RemoveEncryptionInfo(kSharingFCMAppID,
                                            /*authorized_entity=*/std::string(),
                                            /*callback=*/base::DoNothing());
}

}  // namespace

// static
SharingService* IOSSharingServiceFactory::GetForProfile(ProfileIOS* profile) {
  CHECK(!profile->IsOffTheRecord());
  return static_cast<SharingService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
SharingService* IOSSharingServiceFactory::GetForProfileIfExists(
    ProfileIOS* profile) {
  return static_cast<SharingService*>(
      GetInstance()->GetServiceForBrowserState(profile, false));
}

// static
IOSSharingServiceFactory* IOSSharingServiceFactory::GetInstance() {
  static base::NoDestructor<IOSSharingServiceFactory> instance;
  return instance.get();
}

IOSSharingServiceFactory::IOSSharingServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SharingService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeInstanceIDProfileServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
  DependsOn(IOSSharingMessageBridgeFactory::GetInstance());
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(ios::FaviconServiceFactory::GetInstance());
}

IOSSharingServiceFactory::~IOSSharingServiceFactory() {}

std::unique_ptr<KeyedService> IOSSharingServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSPushNotifications)) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);

  if (!sync_service) {
    return nullptr;
  }

  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  auto sync_prefs = std::make_unique<SharingSyncPreference>(
      profile->GetPrefs(), device_info_sync_service);

  auto vapid_key_manager =
      std::make_unique<VapidKeyManager>(sync_prefs.get(), sync_service);

  instance_id::InstanceIDProfileService* instance_id_service =
      IOSChromeInstanceIDProfileServiceFactory::GetForProfile(profile);
  auto sharing_device_registration =
      std::make_unique<IOSSharingDeviceRegistrationImpl>(
          profile->GetPrefs(), sync_prefs.get(), vapid_key_manager.get(),
          instance_id_service->driver(), sync_service);

  SharingMessageBridge* message_bridge =
      IOSSharingMessageBridgeFactory::GetForProfile(profile);
  gcm::GCMDriver* gcm_driver =
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile)->driver();
  CleanEncryptionInfoWithoutAuthorizedEntity(gcm_driver);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  syncer::LocalDeviceInfoProvider* local_device_info_provider =
      device_info_sync_service->GetLocalDeviceInfoProvider();
  auto fcm_sender = std::make_unique<SharingFCMSender>(
      /*web_push_sender=*/nullptr, message_bridge, sync_prefs.get(),
      vapid_key_manager.get(), gcm_driver, device_info_tracker,
      local_device_info_provider, sync_service);
  SharingFCMSender* fcm_sender_ptr = fcm_sender.get();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      web::GetUIThreadTaskRunner({});
  auto sharing_message_sender = std::make_unique<SharingMessageSender>(
      local_device_info_provider, task_runner);
  auto ios_push_sender =
      std::make_unique<sharing_message::SharingIOSPushSender>(
          message_bridge, device_info_tracker, local_device_info_provider,
          sync_service);
  sharing_message_sender->RegisterSendDelegate(
      SharingMessageSender::DelegateType::kIOSPush, std::move(ios_push_sender));

  auto device_source = std::make_unique<SharingDeviceSourceSync>(
      sync_service, local_device_info_provider, device_info_tracker);

  auto handler_registry = std::make_unique<IOSSharingHandlerRegistryImpl>(
      sharing_message_sender.get());

  auto fcm_handler = std::make_unique<SharingFCMHandler>(
      gcm_driver, device_info_tracker, fcm_sender_ptr, handler_registry.get());

  favicon::FaviconService* favicon_service =
      ios::FaviconServiceFactory::GetForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS);

  send_tab_to_self::SendTabToSelfModel* send_tab_model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();

  return std::make_unique<SharingService>(
      std::move(sync_prefs), std::move(vapid_key_manager),
      std::move(sharing_device_registration), std::move(sharing_message_sender),
      std::move(device_source), std::move(handler_registry),
      std::move(fcm_handler), sync_service, favicon_service, send_tab_model,
      std::move(task_runner));
}

bool IOSSharingServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}
