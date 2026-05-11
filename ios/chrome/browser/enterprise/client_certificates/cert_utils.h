// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_

#import <string_view>

#import "components/enterprise/client_certificates/core/private_key_factory.h"

class PrefService;
namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace policy {
class DeviceManagementService;
}  // namespace policy

namespace client_certificates {

// The application tag used to identify the browser-level certificate store.
inline constexpr std::string_view kBrowserLevelApplicationTag = "browser";

class CertificateProvisioningServiceIOS;
class CertificateStore;

// Returns the keychain access group used to isolate keys for Device Trust.
// Must be called on a background thread to prevent hangs.
std::optional<std::string> GetAccessGroup();

// Sets a hook to override the keychain access group in tests.
void SetAccessGroupHookForTesting(std::optional<std::string> (*func)());

// Creates a PrivateKeyFactory using the given `application_tag` to isolate
// keys. For browser-level usage, `kBrowserLevelApplicationTag` should be
// provided. For profile-level usage, the profile name should be provided.
std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory(
    std::string_view application_tag);

// Deletes the client certificate keys from the Keychain associated with the
// given `profile_name`. Must be called on a background thread.
void DeleteClientCertificateKeys(std::string_view profile_name);

std::unique_ptr<client_certificates::CertificateProvisioningServiceIOS>
CreateBrowserCertificateProvisioningService(
    PrefService* local_state,
    client_certificates::CertificateStore* certificate_store,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

}  // namespace client_certificates

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERT_UTILS_H_
