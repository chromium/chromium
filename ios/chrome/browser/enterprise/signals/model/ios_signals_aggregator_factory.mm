// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "components/device_signals/core/browser/signals_aggregator_impl.h"
#import "components/device_signals/core/browser/signals_collector.h"
#import "components/device_signals/core/browser/user_permission_service.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_system_signals_collector.h"
#import "ios/chrome/browser/enterprise/signals/model/profile_signals_collector_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

// Implementation of UserPermissionService for iOS enterprise reporting.
// On iOS, enterprise signals collection is governed entirely by enterprise
// management policies (such as managed profile reporting or CBCM) and is only
// triggered in managed contexts. Unlike desktop platforms, there is no
// interactive user consent flow for signals collection on iOS. Consent is
// implicitly established when using a managed profile or device where an
// administrator has enabled reporting policies. Therefore, permission checks
// automatically return `kGranted` without requiring explicit user consent.
class IOSUserPermissionService : public device_signals::UserPermissionService {
 public:
  IOSUserPermissionService() = default;
  ~IOSUserPermissionService() override = default;

  // device_signals::UserPermissionService:
  // Returns false since iOS does not support an interactive user consent flow
  // for device signals collection.
  bool ShouldCollectConsent() const override { return false; }

  // Permissions are automatically granted because signals collection on iOS is
  // restricted to managed contexts where administrative policies govern
  // reporting, implicitly establishing user consent.
  device_signals::UserPermission CanCollectSignals() const override {
    return device_signals::UserPermission::kGranted;
  }
  device_signals::UserPermission CanCollectReportSignals() const override {
    return device_signals::UserPermission::kGranted;
  }
  bool HasUserConsented() const override { return true; }
};

device_signals::UserPermissionService* GetIOSUserPermissionService() {
  static base::NoDestructor<IOSUserPermissionService> instance;
  return instance.get();
}

}  // namespace

IOSSignalsAggregatorFactory::IOSSignalsAggregatorFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "IOSSignalsAggregator") {
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(ReauthenticationServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSSignalsAggregatorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  std::vector<std::unique_ptr<device_signals::SignalsCollector>> collectors;

  BrowserPolicyConnectorIOS* browser_policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  CHECK(browser_policy_connector);
  // The machine-level manager is created at startup, before CBCM policy is
  // fetched; pass it unconditionally and let the collector read the store at
  // collection time. Gating on management authority here would freeze a
  // null pointer for the first session.
  collectors.push_back(std::make_unique<IOSSystemSignalsCollector>(
      base::BindRepeating(&BrowserPolicyConnectorIOS::GetDeviceAffiliationIds,
                          base::Unretained(browser_policy_connector)),
      browser_policy_connector->machine_level_user_cloud_policy_manager(),
      ReauthenticationServiceFactory::GetForProfile(profile)
          ->GetReauthModule()));

  collectors.push_back(std::make_unique<ProfileSignalsCollectorIOS>(
      profile->GetPrefs(), profile->GetUserCloudPolicyManager(),
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile),
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(profile)));

  return std::make_unique<device_signals::SignalsAggregatorImpl>(
      GetIOSUserPermissionService(), std::move(collectors));
}
