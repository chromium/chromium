// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory_ios.h"

#import "base/no_destructor.h"
#import "components/enterprise/client_certificates/core/dm_server_client.h"
#import "components/enterprise/client_certificates/core/features.h"
#import "components/enterprise/client_certificates/core/key_upload_client.h"
#import "components/enterprise/client_certificates/core/profile_cloud_management_delegate.h"
#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "ios/chrome/browser/enterprise/client_certificates/certificate_store_factory.h"
#import "ios/chrome/browser/enterprise/client_certificates/profile_context_delegate_ios.h"
#import "ios/chrome/browser/enterprise/core/dependency_factory_impl_ios.h"
#import "ios/chrome/browser/enterprise/identifiers/profile_id_service_factory_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace client_certificates {

namespace {

policy::DeviceManagementService* GetDeviceManagementService() {
  BrowserPolicyConnectorIOS* connector =
      GetApplicationContext()->GetBrowserPolicyConnector();
  return connector ? connector->device_management_service() : nullptr;
}

}  // namespace

// static
CertificateProvisioningServiceFactoryIOS*
CertificateProvisioningServiceFactoryIOS::GetInstance() {
  static base::NoDestructor<CertificateProvisioningServiceFactoryIOS> instance;
  return instance.get();
}

// static
CertificateProvisioningServiceIOS*
CertificateProvisioningServiceFactoryIOS::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<CertificateProvisioningServiceIOS>(
          profile, /*create=*/true);
}

CertificateProvisioningServiceFactoryIOS::
    CertificateProvisioningServiceFactoryIOS()
    : ProfileKeyedServiceFactoryIOS("IOSCertificateProvisioningService",
                                    ProfileSelection::kNoInstanceInIncognito,
                                    ServiceCreation::kCreateWithProfile,
                                    TestingCreation::kNoServiceForTests) {
  DependsOn(CertificateStoreFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactoryIOS::GetInstance());
}

CertificateProvisioningServiceFactoryIOS::
    ~CertificateProvisioningServiceFactoryIOS() = default;

std::unique_ptr<KeyedService>
CertificateProvisioningServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!features::IsClientCertificateProvisioningOnIOSEnabled()) {
    return nullptr;
  }

  auto* certificate_store = CertificateStoreFactory::GetForProfile(profile);
  auto url_loader_factory = profile->GetSharedURLLoaderFactory();
  auto* device_management_service = GetDeviceManagementService();
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactoryIOS::GetForProfile(profile);
  if (!certificate_store || !url_loader_factory || !device_management_service ||
      !profile_id_service) {
    return nullptr;
  }

  auto core_service = CertificateProvisioningService::Create(
      profile->GetPrefs(), certificate_store,
      std::make_unique<ProfileContextDelegateIOS>(),
      KeyUploadClient::Create(
          std::make_unique<
              enterprise_attestation::ProfileCloudManagementDelegate>(
              std::make_unique<enterprise_core::DependencyFactoryImplIOS>(
                  profile),
              profile_id_service,
              enterprise_attestation::DMServerClient::Create(
                  device_management_service, std::move(url_loader_factory)))));
  return client_certificates::CertificateProvisioningServiceIOS::Create(
      std::move(core_service));
}

}  // namespace client_certificates
