// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_PROVISIONING_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_PROVISIONING_SERVICE_FACTORY_IOS_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace client_certificates {

class CertificateProvisioningServiceIOS;

class CertificateProvisioningServiceFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static CertificateProvisioningServiceFactoryIOS* GetInstance();
  static CertificateProvisioningServiceIOS* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<CertificateProvisioningServiceFactoryIOS>;

  CertificateProvisioningServiceFactoryIOS();
  ~CertificateProvisioningServiceFactoryIOS() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_PROVISIONING_SERVICE_FACTORY_IOS_H_
