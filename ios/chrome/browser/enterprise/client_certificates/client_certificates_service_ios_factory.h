// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_

#import <memory>

#import "ios/chrome/browser/enterprise/client_certificates/client_certificates_service_ios.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace client_certificates {

class ClientCertificatesServiceIOSFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          ClientCertificatesServiceIOSFactory,
          ClientCertificatesServiceIOS> {
 public:
  explicit ClientCertificatesServiceIOSFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_FACTORY_H_
