// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/time/default_clock.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/signin/public/base/device_id_helper.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_sync_service_impl.h"
#include "components/sync_device_info/local_device_info_provider_impl.h"
#include "components/version_info/version_info.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#include "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  absl::optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override {
    return absl::nullopt;
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

  absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
  GetPhoneAsASecurityKeyInfo() const override {
    return absl::nullopt;
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
  DependsOn(WebViewModelTypeStoreServiceFactory::GetInstance());
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
      WebViewModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory(),
      std::move(local_device_info_provider), std::move(device_prefs),
      std::move(device_info_sync_client), sync_invalidations_service);
}

}  // namespace ios_web_view
