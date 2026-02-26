// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios_factory.h"

#import "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"
#import "ios/chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace client_certificates {

// static
ClientCertificatesServiceIOSFactory*
ClientCertificatesServiceIOSFactory::GetInstance() {
  static base::NoDestructor<ClientCertificatesServiceIOSFactory> instance;
  return instance.get();
}

// static
ClientCertificatesServiceIOS*
ClientCertificatesServiceIOSFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ClientCertificatesServiceIOS>(
      profile, /*create=*/true);
}

ClientCertificatesServiceIOSFactory::ClientCertificatesServiceIOSFactory()
    : ProfileKeyedServiceFactoryIOS("ClientCertificatesServiceIOS",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(CertificateProvisioningServiceFactoryIOS::GetInstance());
}

ClientCertificatesServiceIOSFactory::~ClientCertificatesServiceIOSFactory() =
    default;

std::unique_ptr<KeyedService>
ClientCertificatesServiceIOSFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  CertificateProvisioningServiceIOS* profile_service =
      CertificateProvisioningServiceFactoryIOS::GetForProfile(profile);

  CertificateProvisioningServiceIOS* browser_service =
      static_cast<CertificateProvisioningServiceIOS*>(
          GetApplicationContext()
              ->GetBrowserPolicyConnector()
              ->chrome_browser_cloud_management_controller()
              ->GetCertificateProvisioningService());

  return ClientCertificatesServiceIOS::Create(profile, profile_service,
                                              browser_service);
}

}  // namespace client_certificates
