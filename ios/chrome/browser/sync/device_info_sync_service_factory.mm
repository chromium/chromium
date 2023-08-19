// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/memory/singleton.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/send_tab_to_self/features.h"
#import "components/signin/public/base/device_id_helper.h"
#import "components/sync/invalidations/sync_invalidations_service.h"
#import "components/sync/model/model_type_store_service.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/sync_device_info/device_info_sync_client.h"
#import "components/sync_device_info/device_info_sync_service_impl.h"
#import "components/sync_device_info/local_device_info_provider_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/sync/model_type_store_service_factory.h"
#import "ios/chrome/browser/sync/sync_invalidations_service_factory.h"
#import "ios/chrome/common/channel_info.h"

namespace {

class DeviceInfoSyncClient : public syncer::DeviceInfoSyncClient {
 public:
  DeviceInfoSyncClient(
      PrefService* prefs,
      syncer::SyncInvalidationsService* sync_invalidations_service)
      : prefs_(prefs),
        sync_invalidations_service_(sync_invalidations_service) {}
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
  absl::optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override {
    return absl::nullopt;
  }

  // syncer::DeviceInfoSyncClient:
  syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
  GetPhoneAsASecurityKeyInfo() const override {
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  // syncer::DeviceInfoSyncClient:
  absl::optional<std::string> GetFCMRegistrationToken() const override {
    if (sync_invalidations_service_) {
      return sync_invalidations_service_->GetFCMRegistrationToken();
    }
    // If the service is not enabled, then the registration token must be empty,
    // not unknown (absl::nullopt). This is needed to reset previous token if
    // the invalidations have been turned off.
    return std::string();
  }

  // syncer::DeviceInfoSyncClient:
  absl::optional<syncer::ModelTypeSet> GetInterestedDataTypes() const override {
    if (sync_invalidations_service_) {
      return sync_invalidations_service_->GetInterestedDataTypes();
    }
    // If the service is not enabled, then the list of types must be empty, not
    // unknown (absl::nullopt). This is needed to reset previous types if the
    // invalidations have been turned off.
    return syncer::ModelTypeSet();
  }

  // syncer::DeviceInfoSyncClient:
  // Returns false since we only care about Chrome OS devices
  bool IsUmaEnabledOnCrOSDevice() const override { return false; }

 private:
  PrefService* const prefs_;
  syncer::SyncInvalidationsService* const sync_invalidations_service_;
};

}  // namespace

// static
syncer::DeviceInfoSyncService* DeviceInfoSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
DeviceInfoSyncServiceFactory* DeviceInfoSyncServiceFactory::GetInstance() {
  return base::Singleton<DeviceInfoSyncServiceFactory>::get();
}

// static
void DeviceInfoSyncServiceFactory::GetAllDeviceInfoTrackers(
    std::vector<const syncer::DeviceInfoTracker*>* trackers) {
  DCHECK(trackers);
  std::vector<ChromeBrowserState*> browser_state_list =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();
  for (ChromeBrowserState* browser_state : browser_state_list) {
    syncer::DeviceInfoSyncService* service =
        DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state);
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
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
}

DeviceInfoSyncServiceFactory::~DeviceInfoSyncServiceFactory() {}

std::unique_ptr<KeyedService>
DeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::SyncInvalidationsService* const sync_invalidations_service =
      SyncInvalidationsServiceFactory::GetForBrowserState(browser_state);
  auto device_info_sync_client = std::make_unique<DeviceInfoSyncClient>(
      browser_state->GetPrefs(), sync_invalidations_service);
  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          ::GetChannel(), ::GetVersionString(), device_info_sync_client.get());
  auto device_prefs = std::make_unique<syncer::DeviceInfoPrefs>(
      browser_state->GetPrefs(), base::DefaultClock::GetInstance());

  return std::make_unique<syncer::DeviceInfoSyncServiceImpl>(
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client), sync_invalidations_service);
}
