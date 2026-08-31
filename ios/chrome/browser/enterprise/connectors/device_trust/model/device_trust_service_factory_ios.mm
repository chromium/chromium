// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_service_factory_ios.h"

#import <memory>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/no_destructor.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/device_trust/core/attestation/attester.h"
#import "components/enterprise/device_trust/core/attestation/browser_attestation_service.h"
#import "components/enterprise/device_trust/core/attestation/device_attester.h"
#import "components/enterprise/device_trust/core/attestation/profile_attester.h"
#import "components/enterprise/device_trust/core/device_trust_connector_service.h"
#import "components/enterprise/device_trust/core/device_trust_service.h"
#import "components/enterprise/device_trust/core/signals/decorators/common/signals_aggregator_decorator.h"
#import "components/enterprise/device_trust/core/signals/signals_filterer.h"
#import "components/enterprise/device_trust/core/signals/signals_service_impl.h"
#import "components/policy/core/common/cloud/cloud_policy_core.h"
#import "components/policy/core/common/cloud/cloud_policy_store.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_connector_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_key_manager_ios.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {

bool IsProfileManaged(ProfileIOS* profile) {
  policy::BrowserManagementService* management_service =
      policy::BrowserManagementServiceFactory::GetForProfile(profile);
  return management_service && management_service->IsManaged();
}

DeviceTrustKeyManagerIOS* GetDeviceTrustKeyManager() {
  static base::NoDestructor<DeviceTrustKeyManagerIOS> key_manager;
  return key_manager.get();
}

policy::CloudPolicyStore* GetBrowserCloudPolicyStore() {
  BrowserPolicyConnectorIOS* browser_policy_connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  if (!browser_policy_connector) {
    return nullptr;
  }

  policy::MachineLevelUserCloudPolicyManager* policy_manager =
      browser_policy_connector->machine_level_user_cloud_policy_manager();
  if (!policy_manager || !policy_manager->core()) {
    return nullptr;
  }

  return policy_manager->core()->store();
}

policy::CloudPolicyStore* GetUserCloudPolicyStore(ProfileIOS* profile) {
  policy::UserCloudPolicyManager* policy_manager =
      profile->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->core()) {
    return nullptr;
  }

  return policy_manager->core()->store();
}

std::unique_ptr<KeyedService> CreateDeviceTrustService(ProfileIOS* profile) {
  if (!IsProfileManaged(profile)) {
    return nullptr;
  }

  enterprise_connectors::DeviceTrustConnectorService* connector_service =
      DeviceTrustConnectorServiceFactoryIOS::GetForProfile(profile);

  device_signals::SignalsAggregator* signals_aggregator =
      IOSSignalsAggregatorFactory::GetForProfile(profile);

  enterprise::ProfileIdService* profile_id_service =
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile);

  if (!connector_service || !signals_aggregator || !profile_id_service) {
    return nullptr;
  }

  std::vector<std::unique_ptr<enterprise_connectors::SignalsDecorator>>
      decorators;
  decorators.push_back(
      std::make_unique<enterprise_connectors::SignalsAggregatorDecorator>(
          signals_aggregator));

  std::unique_ptr<enterprise_connectors::SignalsService> signals_service =
      std::make_unique<enterprise_connectors::SignalsServiceImpl>(
          std::move(decorators),
          std::make_unique<enterprise_connectors::SignalsFilterer>());

  std::vector<std::unique_ptr<enterprise_connectors::Attester>> attesters;
  attesters.push_back(std::make_unique<enterprise_connectors::DeviceAttester>(
      GetDeviceTrustKeyManager(), policy::BrowserDMTokenStorage::Get(),
      GetBrowserCloudPolicyStore()));
  attesters.push_back(std::make_unique<enterprise_connectors::ProfileAttester>(
      profile_id_service, GetUserCloudPolicyStore(profile)));

  std::unique_ptr<enterprise_connectors::AttestationService>
      attestation_service =
          std::make_unique<enterprise_connectors::BrowserAttestationService>(
              std::move(attesters),
              enterprise_connectors::VerifiedAccessFlow::CBCM);

  return std::make_unique<enterprise_connectors::DeviceTrustService>(
      std::move(attestation_service), std::move(signals_service),
      connector_service);
}

}  // namespace

// static
enterprise_connectors::DeviceTrustService*
DeviceTrustServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<enterprise_connectors::DeviceTrustService>(
          profile, /*create=*/true);
}

// static
DeviceTrustServiceFactoryIOS* DeviceTrustServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<DeviceTrustServiceFactoryIOS> instance;
  return instance.get();
}

// static
ProfileKeyedServiceFactoryIOS::TestingFactory
DeviceTrustServiceFactoryIOS::GetDefaultFactory() {
  return base::BindOnce(&CreateDeviceTrustService);
}

DeviceTrustServiceFactoryIOS::DeviceTrustServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("DeviceTrustService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(DeviceTrustConnectorServiceFactoryIOS::GetInstance());
  DependsOn(policy::BrowserManagementServiceFactory::GetInstance());
  DependsOn(IOSSignalsAggregatorFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

DeviceTrustServiceFactoryIOS::~DeviceTrustServiceFactoryIOS() = default;

std::unique_ptr<KeyedService>
DeviceTrustServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return CreateDeviceTrustService(profile);
}
