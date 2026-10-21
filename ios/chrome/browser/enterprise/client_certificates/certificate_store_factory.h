// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_

#import "components/enterprise/client_certificates/core/certificate_store.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace client_certificates {

class CertificateStoreFactory final
    : public TypedProfileKeyedServiceFactoryIOS<CertificateStoreFactory,
                                                CertificateStore> {
 public:
  explicit CertificateStoreFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
