// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_CERT_UTIL_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_CERT_UTIL_H_

#include <stdint.h>

#include <string_view>

#include "third_party/boringssl/src/include/openssl/x509v3.h"

namespace net::transport_security_state {
class SPKIHash;
}  // namespace net::transport_security_state

// Decodes the PEM block in |pem_data| and attempts to parse the resulting
// structure. Returns a pointer to a X509 instance if successful and NULL
// otherwise.
bssl::UniquePtr<X509> GetX509CertificateFromPEM(std::string_view pem_data);

// Extracts the SubjectPublicKeyInfo from |*certificate| and copies its SHA256
// digest to |*out_hash|. Returns true on success and false on failure.
bool CalculateSPKIHashFromCertificate(
    X509* certificate,
    net::transport_security_state::SPKIHash* out_hash);

// Extracts the name from |*certificate| and copies the result to |*name|.
// Returns true on success and false on failure.
// On success |*name| will contain the Subject's CommonName if available or the
// concatenation |OrganizationName| + " " + |OrganizationalUnitName| otherwise.
bool ExtractSubjectNameFromCertificate(X509* certificate, std::string* name);

// Decodes the PEM block in |pem_key| and sets |*out_hash| to the SHA256 digest
// of the resulting structure. The encoded PEM block in |pem_key| is expected to
// be a SubjectPublicKeyInfo structure. Returns true on success and false on
// failure.
bool CalculateSPKIHashFromKey(
    std::string_view pem_key,
    net::transport_security_state::SPKIHash* out_hash);

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_CERT_UTIL_H_
