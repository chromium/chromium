// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/certificate_store_factory.h"

#import "base/no_destructor.h"
#import "components/enterprise/client_certificates/core/leveldb_certificate_store.h"
#import "ios/chrome/browser/enterprise/client_certificates/cert_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace client_certificates {
// static
CertificateStoreFactory* CertificateStoreFactory::GetInstance() {
  static base::NoDestructor<CertificateStoreFactory> instance;
  return instance.get();
}

// static
CertificateStore* CertificateStoreFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<CertificateStore>(
      profile, /*create=*/true);
}

CertificateStoreFactory::CertificateStoreFactory()
    : ProfileKeyedServiceFactoryIOS("IOSCertificateStore",
                                    ProfileSelection::kNoInstanceInIncognito) {}

CertificateStoreFactory::~CertificateStoreFactory() = default;

std::unique_ptr<KeyedService> CertificateStoreFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!profile->GetProtoDatabaseProvider()) {
    return nullptr;
  }

  return LevelDbCertificateStore::Create(profile->GetStatePath(),
                                         profile->GetProtoDatabaseProvider(),
                                         CreatePrivateKeyFactory());
}

}  // namespace client_certificates
