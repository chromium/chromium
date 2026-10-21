// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/client_certificates/certificate_store_factory.h"

#import <utility>

#import "components/enterprise/client_certificates/core/certificate_store.h"
#import "components/enterprise/client_certificates/core/leveldb_certificate_store.h"
#import "ios/chrome/browser/enterprise/client_certificates/cert_utils.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace client_certificates {

CertificateStoreFactory::CertificateStoreFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "IOSCertificateStore",
          ProfileSelection::kNoInstanceInIncognito) {}

std::unique_ptr<KeyedService> CertificateStoreFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  if (!profile->GetProtoDatabaseProvider()) {
    return nullptr;
  }

  return LevelDbCertificateStore::Create(
      profile->GetStatePath(), profile->GetProtoDatabaseProvider(),
      CreatePrivateKeyFactory(profile->GetProfileName()));
}

}  // namespace client_certificates
