// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_OBJECTS_EXTRACTOR_H_
#define NET_CERT_CT_OBJECTS_EXTRACTOR_H_

#include <string>
#include <string_view>

#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

namespace net::ct {

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.2. See Section 3.3 of
// RFC6962.
inline constexpr uint8_t kEmbeddedSCTOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                              0xD6, 0x79, 0x02, 0x04, 0x02};

struct SignedEntryData;

// Extracts a SignedCertificateTimestampList that has been embedded within a
// leaf cert as an X.509v3 extension with the OID 1.3.6.1.4.1.11129.2.4.2.
// If the extension is present, returns true, updating |*sct_list| to contain
// the encoded list, minus the DER encoding necessary for the extension.
// |*sct_list| can then be further decoded with ct::DecodeSCTList
NET_EXPORT_PRIVATE bool ExtractEmbeddedSCTList(const CRYPTO_BUFFER* cert,
                                               std::string* sct_list);

// Obtains a PrecertChain log entry for |leaf|, an X.509v3 certificate that
// contains an X.509v3 extension with the OID 1.3.6.1.4.1.11129.2.4.2. On
// success, fills |*result| with the data for a PrecertChain log entry and
// returns true.
// The filled |*result| should be verified using ct::CTLogVerifier::Verify
// Note: If |leaf| does not contain the required extension, it is treated as
// a failure.
NET_EXPORT_PRIVATE bool GetPrecertSignedEntry(const CRYPTO_BUFFER* leaf,
                                              const CRYPTO_BUFFER* issuer,
                                              SignedEntryData* result);

// Obtains an X509Chain log entry for |leaf|, an X.509v3 certificate that
// is not expected to contain an X.509v3 extension with the OID
// 1.3.6.1.4.1.11129.2.4.2 (meaning a certificate without an embedded SCT).
// On success, fills |result| with the data for an X509Chain log entry and
// returns true.
// The filled |*result| should be verified using ct::CTLogVerifier::Verify
NET_EXPORT_PRIVATE bool GetX509SignedEntry(const CRYPTO_BUFFER* leaf,
                                           SignedEntryData* result);

// Extracts a SignedCertificateTimestampList that has been embedded within
// an OCSP response as an extension with the OID 1.3.6.1.4.1.11129.2.4.5.
// If the extension is present, and the response matches the issuer and
// serial number, returns true, updating |*sct_list| to contain
// the encoded list, minus the DER encoding necessary for the extension.
// |*sct_list| can then be further decoded with ct::DecodeSCTList.
NET_EXPORT_PRIVATE bool ExtractSCTListFromOCSPResponse(
    const CRYPTO_BUFFER* issuer,
    const std::string& cert_serial_number,
    std::string_view ocsp_response,
    std::string* sct_list);

}  // namespace net::ct

#endif  // NET_CERT_CT_OBJECTS_EXTRACTOR_H_
