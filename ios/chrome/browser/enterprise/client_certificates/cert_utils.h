// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_

#import "components/enterprise/client_certificates/core/private_key_factory.h"

class PrefService;
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace client_certificates {

class CertificateProvisioningServiceIOS;
class CertificateStore;

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory();

std::unique_ptr<client_certificates::CertificateProvisioningServiceIOS>
CreateBrowserCertificateProvisioningService(
    PrefService* local_state,
    client_certificates::CertificateStore* certificate_store,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
