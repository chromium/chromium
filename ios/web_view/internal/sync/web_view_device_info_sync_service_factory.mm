// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/memory/singleton.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/signin/public/base/device_id_helper.h"
#import "components/sync/invalidations/sync_invalidations_service.h"
#import "components/sync/model/data_type_store_service.h"
#import "components/sync/protocol/sync_enums.pb.h"
#import "components/sync_device_info/device_info_prefs.h"
#import "components/sync_device_info/device_info_sync_client.h"
#import "components/sync_device_info/device_info_sync_service_impl.h"
#import "components/sync_device_info/local_device_info_provider_impl.h"
#import "components/version_info/version_info.h"
#import "ios/web_view/internal/sync/web_view_data_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"

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
  bool GetSendTabToSelfReceivingEnabled() const override { return false; }

  // syncer::DeviceInfoSyncClient:
  sync_pb::SyncEnums_SendTabReceivingType GetSendTabToSelfReceivingType()
      const override {
    return sync_pb::
        SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED;
  }

  // syncer::DeviceInfoSyncClient:
  std::optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override {
    return std::nullopt;
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

  syncer::DeviceInfo::PhoneAsASecurityKeyInfo::StatusOrInfo
  GetPhoneAsASecurityKeyInfo() const override {
    return syncer::DeviceInfo::PhoneAsASecurityKeyInfo::NoSupport();
  }

  // syncer::DeviceInfoSyncClient:
  // Returns false since we only care about Chrome OS devices
  bool IsUmaEnabledOnCrOSDevice() const override { return false; }

 private:
  PrefService* const prefs_;
  syncer::SyncInvalidationsService* const sync_invalidations_service_;
};

}  // namespace

namespace ios_web_view {

// static
WebViewDeviceInfoSyncServiceFactory*
WebViewDeviceInfoSyncServiceFactory::GetInstance() {
  return base::Singleton<WebViewDeviceInfoSyncServiceFactory>::get();
}

// static
syncer::DeviceInfoSyncService*
WebViewDeviceInfoSyncServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::DeviceInfoSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewDeviceInfoSyncServiceFactory::WebViewDeviceInfoSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "DeviceInfoSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebViewDataTypeStoreServiceFactory::GetInstance());
  DependsOn(WebViewSyncInvalidationsServiceFactory::GetInstance());
}

WebViewDeviceInfoSyncServiceFactory::~WebViewDeviceInfoSyncServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewDeviceInfoSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  syncer::SyncInvalidationsService* const sync_invalidations_service =
      WebViewSyncInvalidationsServiceFactory::GetForBrowserState(browser_state);
  auto device_info_sync_client = std::make_unique<DeviceInfoSyncClient>(
      browser_state->GetPrefs(), sync_invalidations_service);
  auto local_device_info_provider =
      std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
          version_info::Channel::STABLE,
          std::string(version_info::GetVersionNumber()),
          device_info_sync_client.get());
  auto device_prefs = std::make_unique<syncer::DeviceInfoPrefs>(
      browser_state->GetPrefs(), base::DefaultClock::GetInstance());

  return std::make_unique<syncer::DeviceInfoSyncServiceImpl>(
      WebViewDataTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client), sync_invalidations_service);
}

}  // namespace ios_web_view
