// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_H_

#import "base/functional/callback_forward.h"
#import "components/enterprise/client_certificates/ios/client_identity_ios.h"
#import "components/keyed_service/core/keyed_service.h"

class GURL;
class ProfileIOS;

namespace client_certificates {

class CertificateProvisioningServiceIOS;

class ClientCertificatesServiceIOS : public KeyedService {
 public:
  // Returns an instance of the service which will aggregate certificates from
  // the managed `profile_certificate_provisioning_service` and
  // `browser_certificate_provisioning_service`.
  static std::unique_ptr<ClientCertificatesServiceIOS> Create(
      ProfileIOS* profile,
      CertificateProvisioningServiceIOS*
          profile_certificate_provisioning_service,
      CertificateProvisioningServiceIOS*
          browser_certificate_provisioning_service);

  ~ClientCertificatesServiceIOS() override = default;

  // Aggregates identities from profile and browser level certificate
  // provisioning services and returns first `ClientIdentityIOS` instance
  // matched by `AutoSelectCertificateForUrls` policy for a given
  // `requesting_url`.
  virtual void GetAutoSelectedIdentity(
      const GURL& requesting_url,
      base::OnceCallback<void(std::unique_ptr<ClientIdentityIOS>)>
          callback) = 0;
};

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CLIENT_CERTIFICATES_SERVICE_IOS_H_
