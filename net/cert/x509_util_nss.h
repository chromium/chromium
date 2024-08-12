// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_NSS_H_
#define NET_CERT_X509_UTIL_NSS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "crypto/scoped_nss_types.h"
#include "net/base/net_export.h"
#include "net/cert/cert_type.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"

typedef struct CERTCertificateStr CERTCertificate;
typedef struct CERTNameStr CERTName;
typedef struct PK11SlotInfoStr PK11SlotInfo;
typedef struct SECItemStr SECItem;

namespace net::x509_util {

// Returns a span containing the DER encoded certificate data for `nss_cert`.
NET_EXPORT base::span<const uint8_t> CERTCertificateAsSpan(
    const CERTCertificate* nss_cert);

// Returns a span containing the data pointed to by SECItem `item`.
NET_EXPORT base::span<const uint8_t> SECItemAsSpan(const SECItem& item);

// Returns true if two certificate handles refer to identical certificates.
NET_EXPORT bool IsSameCertificate(CERTCertificate* a, CERTCertificate* b);
NET_EXPORT bool IsSameCertificate(CERTCertificate* a, const X509Certificate* b);
NET_EXPORT bool IsSameCertificate(const X509Certificate* a, CERTCertificate* b);
NET_EXPORT bool IsSameCertificate(CERTCertificate* a, const CRYPTO_BUFFER* b);
NET_EXPORT bool IsSameCertificate(const CRYPTO_BUFFER* a, CERTCertificate* b);

// Returns a CERTCertificate handle from the DER-encoded representation. The
// returned value may reference an already existing CERTCertificate object.
// Returns NULL on failure.
NET_EXPORT ScopedCERTCertificate
CreateCERTCertificateFromBytes(base::span<const uint8_t> data);

// Returns a CERTCertificate handle from |cert|. The returned value may
// reference an already existing CERTCertificate object.  Returns NULL on
// failure.
NET_EXPORT ScopedCERTCertificate
CreateCERTCertificateFromX509Certificate(const X509Certificate* cert);

// Returns a vector of CERTCertificates corresponding to |cert| and its
// intermediates (if any). Returns an empty vector on failure.
NET_EXPORT ScopedCERTCertificateList
CreateCERTCertificateListFromX509Certificate(const X509Certificate* cert);

// Specify behavior if an intermediate certificate fails CERTCertificate
// parsing. kFail means the function should return a failure result
// immediately. kIgnore means the invalid intermediate is not added to the
// output container.
enum class InvalidIntermediateBehavior { kFail, kIgnore };

// Returns a vector of CERTCertificates corresponding to |cert| and its
// intermediates (if any). Returns an empty vector if the certificate could not
// be converted. |invalid_intermediate_behavior| specifies behavior if
// intermediates of |cert| could not be converted.
NET_EXPORT ScopedCERTCertificateList
CreateCERTCertificateListFromX509Certificate(
    const X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior);

// Parses all of the certificates possible from |data|. |format| is a
// bit-wise OR of X509Certificate::Format, indicating the possible formats the
// certificates may have been serialized as. If an error occurs, an empty
// collection will be returned.
NET_EXPORT ScopedCERTCertificateList
CreateCERTCertificateListFromBytes(base::span<const uint8_t> data, int format);

// Increments the refcount of |cert| and returns a handle for that reference.
NET_EXPORT ScopedCERTCertificate DupCERTCertificate(CERTCertificate* cert);

// Increments the refcount of each element in |cerst| and returns a list of
// handles for them.
NET_EXPORT ScopedCERTCertificateList
DupCERTCertificateList(const ScopedCERTCertificateList& certs);

// Creates an X509Certificate from |cert|, with intermediates from |chain|.
// Returns NULL on failure.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromCERTCertificate(
    CERTCertificate* cert,
    const std::vector<CERTCertificate*>& chain);

// Creates an X509Certificate with non-standard parsing options.
// Do not use without consulting //net owners.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromCERTCertificate(
    CERTCertificate* cert,
    const std::vector<CERTCertificate*>& chain,
    X509Certificate::UnsafeCreateOptions options);

// Creates an X509Certificate from |cert|, with no intermediates.
// Returns NULL on failure.
NET_EXPORT scoped_refptr<X509Certificate>
CreateX509CertificateFromCERTCertificate(CERTCertificate* cert);

// Creates an X509Certificate for each element in |certs|.
// Returns an empty list on failure.
NET_EXPORT CertificateList CreateX509CertificateListFromCERTCertificates(
    const ScopedCERTCertificateList& certs);

// Obtains the DER encoded certificate data for |cert|. On success, returns
// true and writes the DER encoded certificate to |*der_encoded|.
NET_EXPORT bool GetDEREncoded(CERTCertificate* cert, std::string* der_encoded);

// Obtains the PEM encoded certificate data for |cert|. On success, returns
// true and writes the PEM encoded certificate to |*pem_encoded|.
NET_EXPORT bool GetPEMEncoded(CERTCertificate* cert, std::string* pem_encoded);

// Stores the values of all rfc822Name subjectAltNames from |cert_handle|
// into |names|. If no names are present, clears |names|.
// WARNING: This method does not validate that the rfc822Name is
// properly encoded; it MAY contain embedded NULs or other illegal
// characters; care should be taken to validate the well-formedness
// before using.
NET_EXPORT void GetRFC822SubjectAltNames(CERTCertificate* cert_handle,
                                         std::vector<std::string>* names);

// Stores the values of all Microsoft UPN subjectAltNames from |cert_handle|
// into |names|. If no names are present, clears |names|.
//
// A "Microsoft UPN subjectAltName" is an OtherName value whose type-id
// is equal to 1.3.6.1.4.1.311.20.2.3 (known as either id-ms-san-sc-logon-upn,
// as described in RFC 4556, or as szOID_NT_PRINCIPAL_NAME, as
// documented in Microsoft KB287547).
// The value field is a UTF8String literal.
// For more information:
//   https://www.ietf.org/mail-archive/web/pkix/current/msg03145.html
//   https://www.ietf.org/proceedings/65/slides/pkix-4/sld1.htm
//   https://tools.ietf.org/html/rfc4556
//
// WARNING: This method does not validate that the name is
// properly encoded; it MAY contain embedded NULs or other illegal
// characters; care should be taken to validate the well-formedness
// before using.
NET_EXPORT void GetUPNSubjectAltNames(CERTCertificate* cert_handle,
                                      std::vector<std::string>* names);

// Generates a unique nickname for |nss_cert| based on the |type| and |slot|.
NET_EXPORT std::string GetDefaultUniqueNickname(CERTCertificate* nss_cert,
                                                CertType type,
                                                PK11SlotInfo* slot);

// Returns a name that can be used to represent the principal.  It tries in
// this order: CN, O and OU and returns the first non-empty one found.
// This mirrors net::CertPrincipal::GetDisplayName.
NET_EXPORT std::string GetCERTNameDisplayName(CERTName* name);

// Stores the notBefore and notAfter times from |cert| into |*not_before| and
// |*not_after|, returning true if successful. |not_before| or |not_after| may
// be null.
NET_EXPORT bool GetValidityTimes(CERTCertificate* cert,
                                 base::Time* not_before,
                                 base::Time* not_after);

// Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
// (all zero) fingerprint on failure.
NET_EXPORT SHA256HashValue CalculateFingerprint256(CERTCertificate* cert);

// Prefer using NSSCertDatabase::ImportUserCert. Temporary public for Kcer.
// Import a user certificate. The private key for the user certificate must
// already be installed, the cert will be installed on the same slot, otherwise
// returns ERR_NO_PRIVATE_KEY_FOR_CERT. If the key exists on multiple slots,
// prioritizes the `preferred_slot`. Returns OK or a network error code.
NET_EXPORT int ImportUserCert(CERTCertificate* cert,
                              crypto::ScopedPK11Slot preferred_slot);

}  // namespace net::x509_util

#endif  // NET_CERT_X509_UTIL_NSS_H_
