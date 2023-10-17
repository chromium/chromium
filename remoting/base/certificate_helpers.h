// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CERTIFICATE_HELPERS_H_
#define REMOTING_BASE_CERTIFICATE_HELPERS_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"

namespace net {
class ClientCertStore;
}

namespace remoting {

// Returns a value from the issuer field for certificate selection, in order of
// preference. If the O or OU entries are populated with multiple values, we
// choose the first one. This function should not be used for validation, only
// for logging or determining which certificate to select for validation.
extern std::string GetPreferredIssuerFieldValue(
    const net::X509Certificate& cert);

// The certificate is valid if both are true:
// * The certificate issuer matches |issuer| exactly or the |issuer| is a '*'.
// * |now| is within [valid_start, valid_expiry].
extern bool IsCertificateValid(const std::string& issuer,
                               const base::Time& now,
                               const net::X509Certificate& cert);

// Returns a ClientCertIdentity instance from |client_certs| which best matches
// the |issuer| and |now| values provided. If a match is found, it is removed
// from |client_certs|. nullptr is returned if no match is found.
extern std::unique_ptr<net::ClientCertIdentity> GetBestMatchFromCertificateList(
    const std::string& issuer,
    const base::Time& now,
    net::ClientCertIdentityList& client_certs);

// Returns a platform-specific ClientCertStore instance.
extern std::unique_ptr<net::ClientCertStore> CreateClientCertStoreInstance();

}  // namespace remoting

#endif  // REMOTING_BASE_CERTIFICATE_HELPERS_H_
