// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/enterprise/client_certificates/core/certificate_store.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace client_certificates {

class CertificateStoreFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static CertificateStoreFactory* GetInstance();
  static CertificateStore* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<CertificateStoreFactory>;

  CertificateStoreFactory();
  ~CertificateStoreFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
