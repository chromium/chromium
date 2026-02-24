// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios_factory.h"

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios.h"
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
}

ClientCertificatesServiceIOSFactory::~ClientCertificatesServiceIOSFactory() =
    default;

std::unique_ptr<KeyedService>
ClientCertificatesServiceIOSFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // TODO: provide real values for profile and browser level provisioning
  // services.
  return ClientCertificatesServiceIOS::Create(profile, nullptr, nullptr);
}

}  // namespace client_certificates
