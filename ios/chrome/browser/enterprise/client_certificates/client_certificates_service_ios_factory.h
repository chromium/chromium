// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace client_certificates {

class ClientCertificatesServiceIOS;

class ClientCertificatesServiceIOSFactory
    : public ProfileKeyedServiceFactoryIOS {
 public:
  static ClientCertificatesServiceIOSFactory* GetInstance();
  static ClientCertificatesServiceIOS* GetForProfile(ProfileIOS* profile);

 private:
  friend class base::NoDestructor<ClientCertificatesServiceIOSFactory>;

  ClientCertificatesServiceIOSFactory();
  ~ClientCertificatesServiceIOSFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_
