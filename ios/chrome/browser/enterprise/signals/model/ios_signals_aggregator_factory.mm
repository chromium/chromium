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
#import "ios/chrome/browser/enterprise/signals/model/ios_user_permission_service_factory.h"
#import "ios/chrome/browser/enterprise/signals/model/profile_signals_collector_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

IOSSignalsAggregatorFactory::IOSSignalsAggregatorFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "IOSSignalsAggregator") {
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(IOSUserPermissionServiceFactory::GetInstance());
  DependsOn(ReauthenticationServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
IOSSignalsAggregatorFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  device_signals::UserPermissionService* user_permission_service =
      IOSUserPermissionServiceFactory::GetForProfile(profile);
  if (!user_permission_service) {
    return nullptr;
  }

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
      user_permission_service, std::move(collectors));
}
