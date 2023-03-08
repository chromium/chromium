// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_win.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/free_deleter.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "crypto/capi_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/known_roots.h"
#include "net/cert/known_roots_win.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_win.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if !defined(CERT_TRUST_HAS_WEAK_SIGNATURE)
// This was introduced in Windows 8 / Windows Server 2012, but retroactively
// ported as far back as Windows XP via system update.
#define CERT_TRUST_HAS_WEAK_SIGNATURE 0x00100000
#endif

namespace net {

namespace {

const void* kResultDebugDataKey = &kResultDebugDataKey;

int MapSecurityError(SECURITY_STATUS err) {
  // There are numerous security error codes, but these are the ones we thus
  // far find interesting.
  switch (err) {
    case SEC_E_WRONG_PRINCIPAL:  // Schannel
    case CERT_E_CN_NO_MATCH:  // CryptoAPI
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_E_UNTRUSTED_ROOT:  // Schannel
    case CERT_E_UNTRUSTEDROOT:  // CryptoAPI
    case TRUST_E_CERT_SIGNATURE:  // CryptoAPI. Caused by weak crypto or bad
                                  // signatures, but not differentiable.
      return ERR_CERT_AUTHORITY_INVALID;
    case SEC_E_CERT_EXPIRED:  // Schannel
    case CERT_E_EXPIRED:  // CryptoAPI
      return ERR_CERT_DATE_INVALID;
    case CRYPT_E_NO_REVOCATION_CHECK:
      return ERR_CERT_NO_REVOCATION_MECHANISM;
    case CRYPT_E_REVOCATION_OFFLINE:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case CRYPT_E_REVOKED:  // Schannel and CryptoAPI
      return ERR_CERT_REVOKED;
    case SEC_E_CERT_UNKNOWN:
    case CERT_E_ROLE:
    case NTE_BAD_ALGID:
      return ERR_CERT_INVALID;
    case CERT_E_WRONG_USAGE:
      // TODO(wtc): Should we add ERR_CERT_WRONG_USAGE?
      return ERR_CERT_INVALID;
    // We received an unexpected_message or illegal_parameter alert message
    // from the server.
    case SEC_E_ILLEGAL_MESSAGE:
      return ERR_SSL_PROTOCOL_ERROR;
    case SEC_E_ALGORITHM_MISMATCH:
      return ERR_SSL_VERSION_OR_CIPHER_MISMATCH;
    case SEC_E_INVALID_HANDLE:
      return ERR_UNEXPECTED;
    case SEC_E_OK:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map the errors in the chain_context->TrustStatus.dwErrorStatus returned by
// CertGetCertificateChain to our certificate status flags.
int MapCertChainErrorStatusToCertStatus(DWORD error_status) {
  CertStatus cert_status = 0;

  // We don't include CERT_TRUST_IS_NOT_TIME_NESTED because it's obsolete and
  // we wouldn't consider it an error anyway
  const DWORD kDateInvalidErrors = CERT_TRUST_IS_NOT_TIME_VALID |
                                   CERT_TRUST_CTL_IS_NOT_TIME_VALID;
  if (error_status & kDateInvalidErrors)
    cert_status |= CERT_STATUS_DATE_INVALID;

  const DWORD kAuthorityInvalidErrors = CERT_TRUST_IS_UNTRUSTED_ROOT |
                                        CERT_TRUST_IS_EXPLICIT_DISTRUST |
                                        CERT_TRUST_IS_PARTIAL_CHAIN;
  if (error_status & kAuthorityInvalidErrors)
    cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  if ((error_status & CERT_TRUST_REVOCATION_STATUS_UNKNOWN) &&
      !(error_status & CERT_TRUST_IS_OFFLINE_REVOCATION))
    cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (error_status & CERT_TRUST_IS_OFFLINE_REVOCATION)
    cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (error_status & CERT_TRUST_IS_REVOKED)
    cert_status |= CERT_STATUS_REVOKED;

  const DWORD kWrongUsageErrors = CERT_TRUST_IS_NOT_VALID_FOR_USAGE |
                                  CERT_TRUST_CTL_IS_NOT_VALID_FOR_USAGE;
  if (error_status & kWrongUsageErrors) {
    // TODO(wtc): Should we add CERT_STATUS_WRONG_USAGE?
    cert_status |= CERT_STATUS_INVALID;
  }

  if (error_status & CERT_TRUST_IS_NOT_SIGNATURE_VALID) {
    // Check for a signature that does not meet the OS criteria for strong
    // signatures.
    // Note: These checks may be more restrictive than the current weak key
    // criteria implemented within CertVerifier, such as excluding SHA-1 or
    // excluding RSA keys < 2048 bits. However, if the user has configured
    // these more stringent checks, respect that configuration and err on the
    // more restrictive criteria.
    if (error_status & CERT_TRUST_HAS_WEAK_SIGNATURE) {
      cert_status |= CERT_STATUS_WEAK_KEY;
    } else {
      cert_status |= CERT_STATUS_AUTHORITY_INVALID;
    }
  }

  // The rest of the errors.
  const DWORD kCertInvalidErrors =
      CERT_TRUST_IS_CYCLIC |
      CERT_TRUST_INVALID_EXTENSION |
      CERT_TRUST_INVALID_POLICY_CONSTRAINTS |
      CERT_TRUST_INVALID_BASIC_CONSTRAINTS |
      CERT_TRUST_INVALID_NAME_CONSTRAINTS |
      CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID |
      CERT_TRUST_HAS_NOT_SUPPORTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_DEFINED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_NOT_PERMITTED_NAME_CONSTRAINT |
      CERT_TRUST_HAS_EXCLUDED_NAME_CONSTRAINT |
      CERT_TRUST_NO_ISSUANCE_CHAIN_POLICY |
      CERT_TRUST_HAS_NOT_SUPPORTED_CRITICAL_EXT;
  if (error_status & kCertInvalidErrors)
    cert_status |= CERT_STATUS_INVALID;

  return cert_status;
}

// Returns true if any common name in the certificate's Subject field contains
// a NULL character.
bool CertSubjectCommonNameHasNull(PCCERT_CONTEXT cert) {
  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = crypto::CryptAlloc;
  decode_para.pfnFree = crypto::CryptFree;
  CERT_NAME_INFO* name_info = nullptr;
  DWORD name_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           WINCRYPT_X509_NAME,
                           cert->pCertInfo->Subject.pbData,
                           cert->pCertInfo->Subject.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &name_info,
                           &name_info_size);
  if (rv) {
    std::unique_ptr<CERT_NAME_INFO, base::FreeDeleter> scoped_name_info(
        name_info);

    // The Subject field may have multiple common names.  According to the
    // "PKI Layer Cake" paper, CryptoAPI uses every common name in the
    // Subject field, so we inspect every common name.
    //
    // From RFC 5280:
    // X520CommonName ::= CHOICE {
    //       teletexString     TeletexString   (SIZE (1..ub-common-name)),
    //       printableString   PrintableString (SIZE (1..ub-common-name)),
    //       universalString   UniversalString (SIZE (1..ub-common-name)),
    //       utf8String        UTF8String      (SIZE (1..ub-common-name)),
    //       bmpString         BMPString       (SIZE (1..ub-common-name)) }
    //
    // We also check IA5String and VisibleString.
    for (DWORD i = 0; i < name_info->cRDN; ++i) {
      PCERT_RDN rdn = &name_info->rgRDN[i];
      for (DWORD j = 0; j < rdn->cRDNAttr; ++j) {
        PCERT_RDN_ATTR rdn_attr = &rdn->rgRDNAttr[j];
        if (strcmp(rdn_attr->pszObjId, szOID_COMMON_NAME) == 0) {
          switch (rdn_attr->dwValueType) {
            // After the CryptoAPI ASN.1 security vulnerabilities described in
            // http://www.microsoft.com/technet/security/Bulletin/MS09-056.mspx
            // were patched, we get CERT_RDN_ENCODED_BLOB for a common name
            // that contains a NULL character.
            case CERT_RDN_ENCODED_BLOB:
              break;
            // Array of 8-bit characters.
            case CERT_RDN_PRINTABLE_STRING:
            case CERT_RDN_TELETEX_STRING:
            case CERT_RDN_IA5_STRING:
            case CERT_RDN_VISIBLE_STRING:
              for (DWORD k = 0; k < rdn_attr->Value.cbData; ++k) {
                if (rdn_attr->Value.pbData[k] == '\0')
                  return true;
              }
              break;
            // Array of 16-bit characters.
            case CERT_RDN_BMP_STRING:
            case CERT_RDN_UTF8_STRING: {
              DWORD num_wchars = rdn_attr->Value.cbData / 2;
              wchar_t* common_name =
                  reinterpret_cast<wchar_t*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_wchars; ++k) {
                if (common_name[k] == L'\0')
                  return true;
              }
              break;
            }
            // Array of ints (32-bit).
            case CERT_RDN_UNIVERSAL_STRING: {
              DWORD num_ints = rdn_attr->Value.cbData / 4;
              int* common_name =
                  reinterpret_cast<int*>(rdn_attr->Value.pbData);
              for (DWORD k = 0; k < num_ints; ++k) {
                if (common_name[k] == 0)
                  return true;
              }
              break;
            }
            default:
              NOTREACHED();
              break;
          }
        }
      }
    }
  }
  return false;
}

// Saves some information about the certificate chain |chain_context| in
// |*verify_result|. The caller MUST initialize |*verify_result| before
// calling this function.
void GetCertChainInfo(PCCERT_CHAIN_CONTEXT chain_context,
                      CertVerifyResult* verify_result) {
  if (chain_context->cChain == 0 || chain_context->rgpChain[0]->cElement == 0) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return;
  }

  PCERT_SIMPLE_CHAIN first_chain = chain_context->rgpChain[0];
  DWORD num_elements = first_chain->cElement;
  PCERT_CHAIN_ELEMENT* element = first_chain->rgpElement;

  PCCERT_CONTEXT verified_cert = nullptr;
  std::vector<PCCERT_CONTEXT> verified_chain;

  // Recheck signatures in the event junk data was provided.
  for (DWORD i = 0; i < num_elements - 1; ++i) {
    PCCERT_CONTEXT issuer = element[i + 1]->pCertContext;

    // If Issuer isn't ECC, skip this certificate.
    if (strcmp(issuer->pCertInfo->SubjectPublicKeyInfo.Algorithm.pszObjId,
               szOID_ECC_PUBLIC_KEY)) {
      continue;
    }

    PCCERT_CONTEXT cert = element[i]->pCertContext;
    if (!CryptVerifyCertificateSignatureEx(
            NULL, X509_ASN_ENCODING, CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT,
            const_cast<PCERT_CONTEXT>(cert), CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
            const_cast<PCERT_CONTEXT>(issuer), 0, nullptr)) {
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    }
  }

  bool has_root_ca = num_elements > 1 &&
      !(chain_context->TrustStatus.dwErrorStatus &
          CERT_TRUST_IS_PARTIAL_CHAIN);

  // Each chain starts with the end entity certificate (i = 0) and ends with
  // either the root CA certificate or the last available intermediate. If a
  // root CA certificate is present, do not inspect the signature algorithm of
  // the root CA certificate because the signature on the trust anchor is not
  // important.
  if (has_root_ca) {
    // If a full chain was constructed, regardless of whether it was trusted,
    // don't inspect the root's signature algorithm.
    num_elements -= 1;
  }

  for (DWORD i = 0; i < num_elements; ++i) {
    PCCERT_CONTEXT cert = element[i]->pCertContext;
    if (i == 0) {
      verified_cert = cert;
    } else {
      verified_chain.push_back(cert);
    }
  }

  if (verified_cert) {
    // Add the root certificate, if present, as it was not added above.
    if (has_root_ca)
      verified_chain.push_back(element[num_elements]->pCertContext);
    scoped_refptr<X509Certificate> verified_cert_with_chain =
        x509_util::CreateX509CertificateFromCertContexts(verified_cert,
                                                         verified_chain);
    if (verified_cert_with_chain)
      verify_result->verified_cert = std::move(verified_cert_with_chain);
    else
      verify_result->cert_status |= CERT_STATUS_INVALID;
  }
}

// Decodes the cert's certificatePolicies extension into a CERT_POLICIES_INFO
// structure and stores it in *output.
void GetCertPoliciesInfo(
    PCCERT_CONTEXT cert,
    std::unique_ptr<CERT_POLICIES_INFO, base::FreeDeleter>* output) {
  PCERT_EXTENSION extension = CertFindExtension(szOID_CERT_POLICIES,
                                                cert->pCertInfo->cExtension,
                                                cert->pCertInfo->rgExtension);
  if (!extension)
    return;

  CRYPT_DECODE_PARA decode_para;
  decode_para.cbSize = sizeof(decode_para);
  decode_para.pfnAlloc = crypto::CryptAlloc;
  decode_para.pfnFree = crypto::CryptFree;
  CERT_POLICIES_INFO* policies_info = nullptr;
  DWORD policies_info_size = 0;
  BOOL rv;
  rv = CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                           szOID_CERT_POLICIES,
                           extension->Value.pbData,
                           extension->Value.cbData,
                           CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                           &decode_para,
                           &policies_info,
                           &policies_info_size);
  if (rv)
    output->reset(policies_info);
}

// Computes the SHA-256 hash of the SPKI of |cert| and stores it in |hash|,
// returning true. If an error occurs, returns false and leaves |hash|
// unmodified.
bool HashSPKI(PCCERT_CONTEXT cert, std::string* hash) {
  base::StringPiece der_bytes(
      reinterpret_cast<const char*>(cert->pbCertEncoded), cert->cbCertEncoded);

  base::StringPiece spki;
  if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki))
    return false;

  *hash = crypto::SHA256HashString(spki);
  return true;
}

bool GetSubject(PCCERT_CONTEXT cert, base::StringPiece* out_subject) {
  base::StringPiece der_bytes(
      reinterpret_cast<const char*>(cert->pbCertEncoded), cert->cbCertEncoded);
  return asn1::ExtractSubjectFromDERCert(der_bytes, out_subject);
}

enum CRLSetResult {
  // Indicates an error happened while attempting to determine CRLSet status.
  // For example, if the certificate's SPKI could not be extracted.
  kCRLSetError,

  // Indicates there is no fresh information about the certificate, or if the
  // CRLSet has expired.
  // In the case of certificate chains, this is only returned if the leaf
  // certificate is not covered by the CRLSet; this is because some
  // intermediates are fully covered, but after filtering, the issuer's CRL
  // is empty and thus omitted from the CRLSet. Since online checking is
  // performed for EV certificates when this status is returned, this would
  // result in needless online lookups for certificates known not-revoked.
  kCRLSetUnknown,

  // Indicates that the certificate (or a certificate in the chain) has been
  // revoked.
  kCRLSetRevoked,

  // The certificate (or certificate chain) has no revocations.
  kCRLSetOk,
};

// Determines if |subject_cert| is revoked within |crl_set|,
// storing the SubjectPublicKeyInfo hash of |subject_cert| in
// |*previous_hash|.
//
// CRLSets store revocations by both SPKI and by the tuple of Issuer SPKI
// Hash & Serial. While |subject_cert| contains enough information to check
// for SPKI revocations, to determine the issuer's SPKI, either |issuer_cert|
// must be supplied, or the hash of the issuer's SPKI provided in
// |*previous_hash|. If |issuer_cert| is omitted, and |*previous_hash| is empty,
// only SPKI checks are performed.
//
// To avoid recomputing SPKI hashes, the hash of |subject_cert| is stored in
// |*previous_hash|. This allows chaining revocation checking, by starting
// at the root and iterating to the leaf, supplying |previous_hash| each time.
//
// In the event of a parsing error, |*previous_hash| is cleared, to prevent the
// wrong Issuer&Serial tuple from being used.
CRLSetResult CheckRevocationWithCRLSet(CRLSet* crl_set,
                                       PCCERT_CONTEXT subject_cert,
                                       PCCERT_CONTEXT issuer_cert,
                                       std::string* previous_hash) {
  DCHECK(crl_set);
  DCHECK(subject_cert);

  // Check to see if |subject_cert|'s SPKI or Subject is revoked.
  std::string subject_hash;
  base::StringPiece subject_name;
  if (!HashSPKI(subject_cert, &subject_hash) ||
      !GetSubject(subject_cert, &subject_name)) {
    NOTREACHED();  // Indicates Windows accepted something irrecoverably bad.
    previous_hash->clear();
    return kCRLSetError;
  }

  if (crl_set->CheckSPKI(subject_hash) == CRLSet::REVOKED ||
      crl_set->CheckSubject(subject_name, subject_hash) == CRLSet::REVOKED) {
    return kCRLSetRevoked;
  }

  // If no issuer cert is provided, nor a hash of the issuer's SPKI, no
  // further checks can be done.
  if (!issuer_cert && previous_hash->empty()) {
    previous_hash->swap(subject_hash);
    return kCRLSetUnknown;
  }

  // Compute the subject's serial.
  const CRYPT_INTEGER_BLOB* serial_blob =
      &subject_cert->pCertInfo->SerialNumber;
  auto serial_bytes = std::make_unique<uint8_t[]>(serial_blob->cbData);
  // The bytes of the serial number are stored little-endian.
  // Note: While MSDN implies that bytes are stripped from this serial,
  // they are not - only CertCompareIntegerBlob actually removes bytes.
  for (DWORD j = 0; j < serial_blob->cbData; j++)
    serial_bytes[j] = serial_blob->pbData[serial_blob->cbData - j - 1];
  base::StringPiece serial(reinterpret_cast<const char*>(serial_bytes.get()),
                           serial_blob->cbData);

  // Compute the issuer's hash. If it was provided (via previous_hash),
  // use that; otherwise, compute it based on |issuer_cert|.
  std::string issuer_hash_local;
  std::string* issuer_hash = previous_hash;
  if (issuer_hash->empty()) {
    if (!HashSPKI(issuer_cert, &issuer_hash_local)) {
      NOTREACHED();  // Indicates Windows accepted something irrecoverably bad.
      previous_hash->clear();
      return kCRLSetError;
    }
    issuer_hash = &issuer_hash_local;
  }

  // Look up by serial & issuer SPKI.
  const CRLSet::Result result = crl_set->CheckSerial(serial, *issuer_hash);
  if (result == CRLSet::REVOKED)
    return kCRLSetRevoked;

  previous_hash->swap(subject_hash);
  if (result == CRLSet::GOOD)
    return kCRLSetOk;
  if (result == CRLSet::UNKNOWN)
    return kCRLSetUnknown;

  NOTREACHED();
  return kCRLSetError;
}

// CheckChainRevocationWithCRLSet attempts to check each element of |chain|
// against |crl_set|. It returns:
//   kCRLSetRevoked: if any element of the chain is known to have been revoked.
//   kCRLSetUnknown: if there is no fresh information about the leaf
//       certificate in the chain or if the CRLSet has expired.
//
//       Only the leaf certificate is considered for coverage because some
//       intermediates have CRLs with no revocations (after filtering) and
//       those CRLs are pruned from the CRLSet at generation time. This means
//       that some EV sites would otherwise take the hit of an OCSP lookup for
//       no reason.
//   kCRLSetOk: otherwise.
CRLSetResult CheckChainRevocationWithCRLSet(PCCERT_CHAIN_CONTEXT chain,
                                            CRLSet* crl_set) {
  if (chain->cChain == 0 || chain->rgpChain[0]->cElement == 0)
    return kCRLSetOk;

  PCERT_CHAIN_ELEMENT* elements = chain->rgpChain[0]->rgpElement;
  DWORD num_elements = chain->rgpChain[0]->cElement;

  bool had_error = false;
  CRLSetResult result = kCRLSetError;
  std::string issuer_spki_hash;
  for (DWORD i = 0; i < num_elements; ++i) {
    PCCERT_CONTEXT subject = elements[num_elements - i - 1]->pCertContext;
    result =
        CheckRevocationWithCRLSet(crl_set, subject, nullptr, &issuer_spki_hash);
    if (result == kCRLSetRevoked)
      return result;
    if (result == kCRLSetError)
      had_error = true;
  }
  if (had_error || crl_set->IsExpired())
    return kCRLSetUnknown;
  return result;
}

void AppendPublicKeyHashesAndUpdateKnownRoot(PCCERT_CHAIN_CONTEXT chain,
                                             HashValueVector* hashes,
                                             bool* known_root) {
  if (chain->cChain == 0)
    return;

  PCERT_SIMPLE_CHAIN first_chain = chain->rgpChain[0];
  PCERT_CHAIN_ELEMENT* const element = first_chain->rgpElement;
  const DWORD num_elements = first_chain->cElement;

  // Walk the chain in reverse, from the probable root to the known leaf, as
  // an optimization for IsKnownRoot checks.
  for (DWORD i = num_elements; i > 0; i--) {
    PCCERT_CONTEXT cert = element[i - 1]->pCertContext;

    base::StringPiece der_bytes(
        reinterpret_cast<const char*>(cert->pbCertEncoded),
        cert->cbCertEncoded);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha256(HASH_VALUE_SHA256);
    crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
    hashes->push_back(sha256);

    if (!*known_root) {
      *known_root =
          GetNetTrustAnchorHistogramIdForSPKI(sha256) != 0 || IsKnownRoot(cert);
    }
  }

  // Reverse the hash list, such that it's ordered from leaf to root.
  std::reverse(hashes->begin(), hashes->end());
}

// Returns true if the certificate is an extended-validation certificate.
//
// This function checks the certificatePolicies extensions of the
// certificates in the certificate chain according to Section 7 (pp. 11-12)
// of the EV Certificate Guidelines Version 1.0 at
// http://cabforum.org/EV_Certificate_Guidelines.pdf.
bool CheckEV(PCCERT_CHAIN_CONTEXT chain_context,
             bool rev_checking_enabled,
             const char* policy_oid) {
  DCHECK_NE(static_cast<DWORD>(0), chain_context->cChain);
  // If the cert doesn't match any of the policies, the
  // CERT_TRUST_IS_NOT_VALID_FOR_USAGE bit (0x10) in
  // chain_context->TrustStatus.dwErrorStatus is set.
  DWORD error_status = chain_context->TrustStatus.dwErrorStatus;

  if (!rev_checking_enabled) {
    // If online revocation checking is disabled then we will have still
    // requested that the revocation cache be checked. However, that will often
    // cause the following two error bits to be set. These error bits mean that
    // the local OCSP/CRL is stale or missing entries for these certificates.
    // Since they are expected, we mask them away.
    error_status &= ~(CERT_TRUST_IS_OFFLINE_REVOCATION |
                      CERT_TRUST_REVOCATION_STATUS_UNKNOWN);
  }
  if (!chain_context->cChain || error_status != CERT_TRUST_NO_ERROR)
    return false;

  // Check the end certificate simple chain (chain_context->rgpChain[0]).
  // If the end certificate's certificatePolicies extension contains the
  // EV policy OID of the root CA, return true.
  PCERT_CHAIN_ELEMENT* element = chain_context->rgpChain[0]->rgpElement;
  int num_elements = chain_context->rgpChain[0]->cElement;
  if (num_elements < 2)
    return false;

  // Look up the EV policy OID of the root CA.
  PCCERT_CONTEXT root_cert = element[num_elements - 1]->pCertContext;
  SHA256HashValue fingerprint = x509_util::CalculateFingerprint256(root_cert);
  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  return metadata->HasEVPolicyOID(fingerprint, policy_oid);
}

// As the revocation parameters passed to CertVerifyProc::VerifyInternal cannot
// be officially smuggled to the Revocation Provider
ABSL_CONST_INIT thread_local CRLSet* thread_local_crl_set = nullptr;

// Custom revocation provider function that compares incoming certificates with
// those in CRLSets. This is called BEFORE the default CRL & OCSP handling
// is invoked (which is handled by the revocation provider function
// "CertDllVerifyRevocation" in cryptnet.dll)
BOOL WINAPI
CertDllVerifyRevocationWithCRLSet(DWORD encoding_type,
                                  DWORD revocation_type,
                                  DWORD num_contexts,
                                  void* rgpvContext[],
                                  DWORD flags,
                                  PCERT_REVOCATION_PARA revocation_params,
                                  PCERT_REVOCATION_STATUS revocation_status) {
  PCERT_CONTEXT* cert_contexts = reinterpret_cast<PCERT_CONTEXT*>(rgpvContext);
  // The dummy CRLSet provider never returns that something is affirmatively
  // *un*revoked, as this would disable other revocation providers from being
  // checked for this certificate (much like an OCSP "Good" status would).
  // Instead, it merely indicates that insufficient information existed to
  // determine if the certificate was revoked (in the good case), or that a cert
  // is affirmatively revoked in the event it appears within the CRLSet.
  // Because of this, set up some basic bookkeeping for the results.
  CHECK(revocation_status);
  revocation_status->dwIndex = 0;
  revocation_status->dwError = static_cast<DWORD>(CRYPT_E_NO_REVOCATION_CHECK);
  revocation_status->dwReason = 0;

  if (num_contexts == 0 || !cert_contexts[0]) {
    SetLastError(static_cast<DWORD>(E_INVALIDARG));
    return FALSE;
  }

  if ((GET_CERT_ENCODING_TYPE(encoding_type) != X509_ASN_ENCODING) ||
      revocation_type != CERT_CONTEXT_REVOCATION_TYPE) {
    SetLastError(static_cast<DWORD>(CRYPT_E_NO_REVOCATION_CHECK));
    return FALSE;
  }

  // No revocation checking possible if there is no associated
  // CRLSet.
  if (!thread_local_crl_set) {
    return FALSE;
  }

  // |revocation_params| is an optional structure; to make life simple and avoid
  // the need to constantly check whether or not it was supplied, create a local
  // copy. If the caller didn't supply anything, it will be empty; otherwise,
  // it will be (non-owning) copies of the caller's original params.
  CERT_REVOCATION_PARA local_params;
  memset(&local_params, 0, sizeof(local_params));
  if (revocation_params) {
    DWORD bytes_to_copy = std::min(revocation_params->cbSize,
                                   static_cast<DWORD>(sizeof(local_params)));
    memcpy(&local_params, revocation_params, bytes_to_copy);
  }
  local_params.cbSize = sizeof(local_params);

  PCERT_CONTEXT subject_cert = cert_contexts[0];

  if ((flags & CERT_VERIFY_REV_CHAIN_FLAG) && num_contexts > 1) {
    // Verifying a chain; first verify from the last certificate in the
    // chain to the first, and then leave the last certificate (which
    // is presumably self-issued, although it may simply be a trust
    // anchor) as the |subject_cert| in order to scan for more
    // revocations.
    std::string issuer_hash;
    PCCERT_CONTEXT issuer_cert = nullptr;
    for (DWORD i = num_contexts; i > 0; --i) {
      subject_cert = cert_contexts[i - 1];
      if (!subject_cert) {
        SetLastError(static_cast<DWORD>(E_INVALIDARG));
        return FALSE;
      }
      CRLSetResult result = CheckRevocationWithCRLSet(
          thread_local_crl_set, subject_cert, issuer_cert, &issuer_hash);
      if (result == kCRLSetRevoked) {
        revocation_status->dwIndex = i - 1;
        revocation_status->dwError = static_cast<DWORD>(CRYPT_E_REVOKED);
        revocation_status->dwReason = CRL_REASON_UNSPECIFIED;
        SetLastError(revocation_status->dwError);
        return FALSE;
      }
      issuer_cert = subject_cert;
    }
    // Verified all certificates from the trust anchor to the leaf, and none
    // were explicitly revoked. Now do a second pass to attempt to determine
    // the issuer for cert_contexts[num_contexts - 1], so that the
    // Issuer SPKI+Serial can be checked for that certificate.
    //
    // This code intentionally ignores the flag
    subject_cert = cert_contexts[num_contexts - 1];
    // Reset local_params.pIssuerCert, since it would contain the issuer
    // for cert_contexts[0].
    local_params.pIssuerCert = nullptr;
    // Fixup the revocation index to point to this cert (in the event it is
    // revoked). If it isn't revoked, this will be done undone later.
    revocation_status->dwIndex = num_contexts - 1;
  }

  // Determine the issuer cert for the incoming cert
  crypto::ScopedPCCERT_CONTEXT issuer_cert;
  if (local_params.pIssuerCert &&
      CryptVerifyCertificateSignatureEx(
          NULL, subject_cert->dwCertEncodingType,
          CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT, subject_cert,
          CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT,
          const_cast<PCERT_CONTEXT>(local_params.pIssuerCert), 0, nullptr)) {
    // Caller has already supplied the issuer cert via the revocation params;
    // just use that.
    issuer_cert.reset(
        CertDuplicateCertificateContext(local_params.pIssuerCert));
  } else if (CertCompareCertificateName(subject_cert->dwCertEncodingType,
                                        &subject_cert->pCertInfo->Subject,
                                        &subject_cert->pCertInfo->Issuer) &&
             CryptVerifyCertificateSignatureEx(
                 NULL, subject_cert->dwCertEncodingType,
                 CRYPT_VERIFY_CERT_SIGN_SUBJECT_CERT, subject_cert,
                 CRYPT_VERIFY_CERT_SIGN_ISSUER_CERT, subject_cert, 0,
                 nullptr)) {
    // Certificate is self-signed; use it as its own issuer.
    issuer_cert.reset(CertDuplicateCertificateContext(subject_cert));
  } else {
    // Scan the caller-supplied stores first, to try and find the issuer cert.
    for (DWORD i = 0; i < local_params.cCertStore && !issuer_cert; ++i) {
      PCCERT_CONTEXT previous_cert = nullptr;
      for (;;) {
        DWORD store_search_flags = CERT_STORE_SIGNATURE_FLAG;
        previous_cert = CertGetIssuerCertificateFromStore(
            local_params.rgCertStore[i], subject_cert, previous_cert,
            &store_search_flags);
        if (!previous_cert)
          break;
        // If a cert is found and meets the criteria, the flag will be reset to
        // zero. Thus NOT having the bit set is equivalent to having found a
        // matching certificate.
        if (!(store_search_flags & CERT_STORE_SIGNATURE_FLAG)) {
          // No need to dupe; reference is held.
          issuer_cert.reset(previous_cert);
          break;
        }
      }
      if (issuer_cert)
        break;
      if (GetLastError() == static_cast<DWORD>(CRYPT_E_SELF_SIGNED)) {
        issuer_cert.reset(CertDuplicateCertificateContext(subject_cert));
        break;
      }
    }

    // At this point, the Microsoft provider opens up the "CA", "Root", and
    // "SPC" stores to search for the issuer certificate, if not found in the
    // caller-supplied stores. It is unclear whether that is necessary here.
  }

  if (!issuer_cert) {
    // Rather than return CRYPT_E_NO_REVOCATION_CHECK (indicating everything
    // is fine to try the next provider), return CRYPT_E_REVOCATION_OFFLINE.
    // This propogates up to the caller as an error while checking revocation,
    // which is the desired intent if there are certificates that cannot
    // be checked.
    revocation_status->dwIndex = 0;
    revocation_status->dwError = static_cast<DWORD>(CRYPT_E_REVOCATION_OFFLINE);
    SetLastError(revocation_status->dwError);
    return FALSE;
  }

  std::string unused;
  CRLSetResult result = CheckRevocationWithCRLSet(
      thread_local_crl_set, subject_cert, issuer_cert.get(), &unused);
  if (result == kCRLSetRevoked) {
    revocation_status->dwError = static_cast<DWORD>(CRYPT_E_REVOKED);
    revocation_status->dwReason = CRL_REASON_UNSPECIFIED;
    SetLastError(revocation_status->dwError);
    return FALSE;
  }

  // The result is ALWAYS FALSE in order to allow the next revocation provider
  // a chance to examine. The only difference is whether or not an error is
  // indicated via dwError (and SetLastError()).
  // Reset the error index so that Windows does not believe this code has
  // examined the entire chain and found no issues until the last cert (thus
  // skipping other revocation providers).
  revocation_status->dwIndex = 0;
  return FALSE;
}

class [[maybe_unused, nodiscard]] ScopedThreadLocalCRLSet {
 public:
  explicit ScopedThreadLocalCRLSet(CRLSet * crl_set)
      : resetter_(&thread_local_crl_set, crl_set) {
    [[maybe_unused]] static bool initialized = [] {
      // Install the CRLSet-based Revocation Provider as the default revocation
      // provider. Because it is installed as a function address (meaning only
      // scoped to the process, and not stored in the registry), it will be used
      // before any registry-based providers, including Microsoft's default
      // provider.
      const CRYPT_OID_FUNC_ENTRY kInterceptFunction[] = {
          {CRYPT_DEFAULT_OID,
           reinterpret_cast<void*>(&CertDllVerifyRevocationWithCRLSet)},
      };
      BOOL ok = CryptInstallOIDFunctionAddress(
          nullptr, X509_ASN_ENCODING, CRYPT_OID_VERIFY_REVOCATION_FUNC,
          std::size(kInterceptFunction), kInterceptFunction,
          CRYPT_INSTALL_OID_FUNC_BEFORE_FLAG);
      DCHECK(ok);
      return true;
    }();
  }
  ~ScopedThreadLocalCRLSet() = default;

 private:
  const base::AutoReset<CRLSet*> resetter_;
};

// Helper class to determine the current version of AuthRoot, as stored in the
// registry. Because calling `RegNotifyChangeKeyValue` associates the event
// with the current thread, and `CertVerifyProc` exists to be used on
// short-lived worker threads, this class handles the thread management to
// ensure a cached, current value is always available.
class AuthRootVersionChecker {
 public:
  struct AuthRootVersion {
    // The sequence number of the CTL.
    // Note: This is sorted big endian, similar to the encoded representation,
    // and not little-endian, like CRYPT_INTEGER_BLOBs are stored by Windows.
    std::vector<uint8_t> sequence_number;

    // The ThisUpdate of the AuthRoot CTL.
    // Note: If the AuthRoot version could not be determined, this will be the
    // default time, and is_null() will return true.
    base::Time this_update;
  };

  // Initializes the AuthRootVersionChecker. Note that this will open a
  // registry key on the current thread, which is expected to be persistent,
  // and begin monitoring for changes. This can be simplified once Windows 7
  // support is dropped.
  AuthRootVersionChecker();

  // Returns the current (potentially cached) version details from the
  // AuthRoot stored in the registry (if any).
  AuthRootVersion GetAuthRootVersion();

 private:
  ~AuthRootVersionChecker() = default;

  // Begins monitoring the registry for subsequent changes (e.g. to refresh
  // the cached value).
  void RefreshWatch();

  // Returns true if the currently cached value may be stale and requires
  // re-processing. If it returns true, the caller is responsible for calling
  // `UpdateAuthRootVersion()` and `RefreshWatch()` (in any order).
  bool ShouldUpdate() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Updates the current AuthRoot version, which may block due to reading
  // the registry and parsing AuthRoot. Should only be called on a worker,
  // and while holding `lock_`.
  void UpdateAuthRootVersion() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::win::RegKey key_;

  // On >= Win 8, the event signalled by Windows whenever the registry has
  // changed.
  const base::win::ScopedHandle event_;

  base::Lock lock_;
  AuthRootVersion auth_root_version_ GUARDED_BY(lock_);

  // On <= Win 7, stores the last update time of the registry key; used to
  // avoid needing to constantly reparse the registry value.
  base::Time last_update_ GUARDED_BY(lock_);
};

AuthRootVersionChecker::AuthRootVersionChecker()
    : event_(CreateEvent(nullptr, FALSE, TRUE, nullptr)) {
  DCHECK(event_.IsValid());

  constexpr wchar_t kAuthRootPath[] =
      L"SOFTWARE\\Microsoft\\SystemCertificates\\AuthRoot\\AutoUpdate";
  if (key_.Open(HKEY_LOCAL_MACHINE, kAuthRootPath, KEY_READ) != ERROR_SUCCESS)
    return;

  // On Win 7, last_update_ is the zero time, and thus will dirty the cache.
  // On Win 8+, event_ is initially signalled, simulating a dirty cache.
}

AuthRootVersionChecker::AuthRootVersion
AuthRootVersionChecker::GetAuthRootVersion() {
  base::AutoLock guard(lock_);

  if (ShouldUpdate()) {
    UpdateAuthRootVersion();
    RefreshWatch();
  }

  return auth_root_version_;
}

void AuthRootVersionChecker::RefreshWatch() {
  // If the registry is corrupted, don't bother.
  if (!key_.Valid())
    return;

  // On Windows, any thread can monitor the registry for changes, and monitoring
  // is not abandoned if the thread ends.
  DWORD flags = REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC;
  RegNotifyChangeKeyValue(key_.Handle(), FALSE, flags, event_.Get(), TRUE);
}

bool AuthRootVersionChecker::ShouldUpdate() {
  lock_.AssertAcquired();

  // If the registry is corrupted, don't bother.
  if (!key_.Valid())
    return false;

  // Check if the event is signaled.
  return WaitForSingleObject(event_.Get(), 0) == WAIT_OBJECT_0;
}

void AuthRootVersionChecker::UpdateAuthRootVersion() {
  lock_.AssertAcquired();

  if (!key_.Valid())
    return;

  constexpr wchar_t kCtlValueName[] = L"EncodedCtl";

  DWORD data_type = REG_BINARY;
  DWORD value_size = 0;
  LONG rv = key_.ReadValue(kCtlValueName, nullptr, &value_size, &data_type);
  if (rv != ERROR_SUCCESS || !value_size || data_type != REG_BINARY)
    return;

  std::vector<uint8_t> value(value_size);
  rv = key_.ReadValue(kCtlValueName, value.data(), &value_size, &data_type);
  if (rv != ERROR_SUCCESS || value_size == 0 || data_type != REG_BINARY)
    return;

  value.resize(value_size);

  crypto::ScopedPCCTL_CONTEXT ctl_context(
      CertCreateCTLContext(PKCS_7_ASN_ENCODING, value.data(), value.size()));
  if (!ctl_context || ctl_context->pCtlInfo->SequenceNumber.cbData == 0)
    return;

  auth_root_version_.sequence_number.assign(
      ctl_context->pCtlInfo->SequenceNumber.pbData,
      ctl_context->pCtlInfo->SequenceNumber.pbData +
          ctl_context->pCtlInfo->SequenceNumber.cbData);
  // Convert from Windows' little-endian representation to the expected (as
  // encoded) big-endian form.
  std::reverse(std::begin(auth_root_version_.sequence_number),
               std::end(auth_root_version_.sequence_number));
  auth_root_version_.this_update =
      base::Time::FromFileTime(ctl_context->pCtlInfo->ThisUpdate);
}

}  // namespace

CertVerifyProcWin::ResultDebugData::ResultDebugData(
    base::Time authroot_this_update,
    std::vector<uint8_t> authroot_sequence_number)
    : authroot_this_update_(authroot_this_update),
      authroot_sequence_number_(std::move(authroot_sequence_number)) {}

CertVerifyProcWin::ResultDebugData::ResultDebugData(
    const ResultDebugData& other) = default;

CertVerifyProcWin::ResultDebugData::~ResultDebugData() = default;

// static
const CertVerifyProcWin::ResultDebugData*
CertVerifyProcWin::ResultDebugData::Get(
    const base::SupportsUserData* debug_data) {
  return static_cast<ResultDebugData*>(
      debug_data->GetUserData(kResultDebugDataKey));
}

// static
void CertVerifyProcWin::ResultDebugData::Create(
    base::Time authroot_this_update,
    std::vector<uint8_t> authroot_sequence_number,
    base::SupportsUserData* debug_data) {
  debug_data->SetUserData(
      kResultDebugDataKey,
      std::make_unique<ResultDebugData>(authroot_this_update,
                                        std::move(authroot_sequence_number)));
}

std::unique_ptr<base::SupportsUserData::Data>
CertVerifyProcWin::ResultDebugData::Clone() {
  return std::make_unique<ResultDebugData>(*this);
}

CertVerifyProcWin::CertVerifyProcWin() = default;

CertVerifyProcWin::~CertVerifyProcWin() = default;

bool CertVerifyProcWin::SupportsAdditionalTrustAnchors() const {
  return false;
}

int CertVerifyProcWin::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  // Ensure the Revocation Provider has been installed and configured for this
  // CRLSet.
  ScopedThreadLocalCRLSet crl_set_scoper(crl_set);

  crypto::ScopedPCCERT_CONTEXT cert_list =
      x509_util::CreateCertContextWithChain(
          cert, x509_util::InvalidIntermediateBehavior::kIgnore);
  if (!cert_list) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

  // Build and validate certificate chain.
  CERT_CHAIN_PARA chain_para;
  memset(&chain_para, 0, sizeof(chain_para));
  chain_para.cbSize = sizeof(chain_para);
  // ExtendedKeyUsage.
  // We still need to request szOID_SERVER_GATED_CRYPTO and szOID_SGC_NETSCAPE
  // today because some certificate chains need them.  IE also requests these
  // two usages.
  static const LPCSTR usage[] = {
    szOID_PKIX_KP_SERVER_AUTH,
    szOID_SERVER_GATED_CRYPTO,
    szOID_SGC_NETSCAPE
  };
  chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
  chain_para.RequestedUsage.Usage.cUsageIdentifier = std::size(usage);
  chain_para.RequestedUsage.Usage.rgpszUsageIdentifier =
      const_cast<LPSTR*>(usage);

  // Get the certificatePolicies extension of the certificate.
  std::unique_ptr<CERT_POLICIES_INFO, base::FreeDeleter> policies_info;
  LPSTR ev_policy_oid = nullptr;
  GetCertPoliciesInfo(cert_list.get(), &policies_info);
  if (policies_info) {
    EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
    for (DWORD i = 0; i < policies_info->cPolicyInfo; ++i) {
      LPSTR policy_oid = policies_info->rgPolicyInfo[i].pszPolicyIdentifier;
      if (metadata->IsEVPolicyOID(policy_oid)) {
        ev_policy_oid = policy_oid;
        chain_para.RequestedIssuancePolicy.dwType = USAGE_MATCH_TYPE_AND;
        chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 1;
        chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier =
            &ev_policy_oid;

        // De-prioritize the CA/Browser forum Extended Validation policy
        // (2.23.140.1.1). See https://crbug.com/705285.
        if (!EVRootCAMetadata::IsCaBrowserForumEvOid(ev_policy_oid))
          break;
      }
    }
  }

  // Revocation checking is always enabled, in order to enable CRLSets to be
  // evaluated as part of a revocation provider. However, when the caller did
  // not explicitly request revocation checking (which is to say, online
  // revocation checking), then only enable cached results. This disables OCSP
  // and CRL fetching, but still allows the revocation provider to be called.
  // Note: The root cert is also checked for revocation status, so that CRLSets
  // will cover revoked SPKIs.
  DWORD chain_flags = CERT_CHAIN_REVOCATION_CHECK_CHAIN;
  bool rev_checking_enabled = (flags & VERIFY_REV_CHECKING_ENABLED);
  if (rev_checking_enabled) {
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
  } else {
    chain_flags |= CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;
  }

  // By default, use the default HCERTCHAINENGINE (aka HCCE_CURRENT_USER). When
  // running tests, use a dynamic HCERTCHAINENGINE. All of the status and cache
  // of verified certificates and chains is tied to the HCERTCHAINENGINE. As
  // each invocation may have changed the set of known roots, invalidate the
  // cache between runs.
  //
  // This is not the most efficient means of doing so; it's possible to mark the
  // Root store used by TestRootCerts as changed, via CertControlStore with the
  // CERT_STORE_CTRL_NOTIFY_CHANGE / CERT_STORE_CTRL_RESYNC, but that's more
  // complexity for what is test-only code.
  crypto::ScopedHCERTCHAINENGINE chain_engine;
  if (TestRootCerts::HasInstance())
    chain_engine = TestRootCerts::GetInstance()->GetChainEngine();

  // Add stapled OCSP response data, which will be preferred over online checks
  // and used when in cache-only mode.
  if (!ocsp_response.empty()) {
    CRYPT_DATA_BLOB ocsp_response_blob;
    ocsp_response_blob.cbData = base::checked_cast<DWORD>(ocsp_response.size());
    ocsp_response_blob.pbData =
        reinterpret_cast<BYTE*>(const_cast<char*>(ocsp_response.data()));
    CertSetCertificateContextProperty(
        cert_list.get(), CERT_OCSP_RESPONSE_PROP_ID,
        CERT_SET_PROPERTY_IGNORE_PERSIST_ERROR_FLAG, &ocsp_response_blob);
  }

  CERT_STRONG_SIGN_SERIALIZED_INFO strong_signed_info;
  memset(&strong_signed_info, 0, sizeof(strong_signed_info));
  strong_signed_info.dwFlags = 0;  // Don't check OCSP or CRL signatures.

  // Note that the following two configurations result in disabling support for
  // any CNG-added algorithms, which may result in some disruption for internal
  // PKI operations that use national forms of crypto (e.g. GOST). However, the
  // fallback mechanism for this (to support SHA-1 chains) will re-enable them,
  // so they should continue to work - just with added latency.
  wchar_t hash_algs[] =
      L"RSA/SHA256;RSA/SHA384;RSA/SHA512;"
      L"ECDSA/SHA256;ECDSA/SHA384;ECDSA/SHA512";
  strong_signed_info.pwszCNGSignHashAlgids = hash_algs;

  // RSA-1024 bit support is intentionally enabled here. More investigation is
  // needed to determine if setting CERT_STRONG_SIGN_DISABLE_END_CHECK_FLAG in
  // the dwStrongSignFlags of |chain_para| would allow the ability to disable
  // support for intermediates/roots < 2048-bits, while still ensuring that
  // end-entity certs signed with SHA-1 are flagged/rejected.
  wchar_t key_sizes[] = L"RSA/1024;ECDSA/256";
  strong_signed_info.pwszCNGPubKeyMinBitLengths = key_sizes;

  CERT_STRONG_SIGN_PARA strong_sign_params;
  memset(&strong_sign_params, 0, sizeof(strong_sign_params));
  strong_sign_params.cbSize = sizeof(strong_sign_params);
  strong_sign_params.dwInfoChoice = CERT_STRONG_SIGN_SERIALIZED_INFO_CHOICE;
  strong_sign_params.pSerializedInfo = &strong_signed_info;

  chain_para.dwStrongSignFlags = 0;
  chain_para.pStrongSignPara = &strong_sign_params;

  PCCERT_CHAIN_CONTEXT chain_context = nullptr;

  // First, try to verify with strong signing enabled. If this fails, or if the
  // chain is rejected, then clear it from |chain_para| so that all subsequent
  // calls will use the fallback path.
  BOOL chain_result =
      CertGetCertificateChain(chain_engine.get(), cert_list.get(),
                              nullptr,  // current system time
                              cert_list->hCertStore, &chain_para, chain_flags,
                              nullptr,  // reserved
                              &chain_context);
  if (chain_result && chain_context &&
      (chain_context->TrustStatus.dwErrorStatus &
       (CERT_TRUST_HAS_WEAK_SIGNATURE | CERT_TRUST_IS_NOT_SIGNATURE_VALID))) {
    // The attempt to verify with strong-sign (only SHA-2) failed, so fall back
    // to disabling it. This will allow SHA-1 chains to be returned, which will
    // then be subsequently signalled as weak if necessary.
    CertFreeCertificateChain(chain_context);
    chain_context = nullptr;

    chain_para.pStrongSignPara = nullptr;
    chain_para.dwStrongSignFlags = 0;
    chain_result =
        CertGetCertificateChain(chain_engine.get(), cert_list.get(),
                                nullptr,  // current system time
                                cert_list->hCertStore, &chain_para, chain_flags,
                                nullptr,  // reserved
                                &chain_context);
  }

  if (!chain_result) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return MapSecurityError(GetLastError());
  }

  // Include diagnostics about the current AuthRoot version.
  static base::NoDestructor<AuthRootVersionChecker> authroot_version_checker;
  AuthRootVersionChecker::AuthRootVersion authroot_version =
      authroot_version_checker->GetAuthRootVersion();
  ResultDebugData::Create(std::move(authroot_version.this_update),
                          std::move(authroot_version.sequence_number),
                          verify_result);

  // Perform a second check with CRLSets. Although the Revocation Provider
  // should have prevented invalid paths from being built, the behaviour and
  // timing of how a Revocation Provider is invoked is not well documented. This
  // is just defense in depth.
  CRLSetResult crl_set_result =
      CheckChainRevocationWithCRLSet(chain_context, crl_set);

  if (crl_set_result == kCRLSetRevoked) {
    verify_result->cert_status |= CERT_STATUS_REVOKED;
  }

  // Even if the cert is possibly EV and crl_set_result == kCRLSetUnknown, we
  // don't check with online revocation checking enabled. See crbug.com/1268848.

  if (chain_context->TrustStatus.dwErrorStatus &
      CERT_TRUST_IS_NOT_VALID_FOR_USAGE) {
    // Could not verify the cert with the EV policy. Remove the EV policy and
    // try again.
    ev_policy_oid = nullptr;
    chain_para.RequestedIssuancePolicy.Usage.cUsageIdentifier = 0;
    chain_para.RequestedIssuancePolicy.Usage.rgpszUsageIdentifier = nullptr;
    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(chain_engine.get(), cert_list.get(),
                                 nullptr,  // current system time
                                 cert_list->hCertStore, &chain_para,
                                 chain_flags,
                                 nullptr,  // reserved
                                 &chain_context)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return MapSecurityError(GetLastError());
    }
  }

  CertVerifyResult temp_verify_result = *verify_result;
  GetCertChainInfo(chain_context, verify_result);
  if (!verify_result->is_issued_by_known_root &&
      (flags & VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS)) {
    *verify_result = temp_verify_result;

    rev_checking_enabled = true;
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    chain_flags &= ~CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY;

    CertFreeCertificateChain(chain_context);
    if (!CertGetCertificateChain(chain_engine.get(), cert_list.get(),
                                 nullptr,  // current system time
                                 cert_list->hCertStore, &chain_para,
                                 chain_flags,
                                 nullptr,  // reserved
                                 &chain_context)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      return MapSecurityError(GetLastError());
    }
    GetCertChainInfo(chain_context, verify_result);
  }

  crypto::ScopedPCCERT_CHAIN_CONTEXT scoped_chain_context(chain_context);

  verify_result->cert_status |= MapCertChainErrorStatusToCertStatus(
      chain_context->TrustStatus.dwErrorStatus);

  // Flag certificates that have a Subject common name with a NULL character.
  if (CertSubjectCommonNameHasNull(cert_list.get()))
    verify_result->cert_status |= CERT_STATUS_INVALID;

  std::u16string hostname16 = base::ASCIIToUTF16(hostname);

  SSL_EXTRA_CERT_CHAIN_POLICY_PARA extra_policy_para;
  memset(&extra_policy_para, 0, sizeof(extra_policy_para));
  extra_policy_para.cbSize = sizeof(extra_policy_para);
  extra_policy_para.dwAuthType = AUTHTYPE_SERVER;
  // Certificate name validation happens separately, later, using an internal
  // routine that has better support for RFC 6125 name matching.
  extra_policy_para.fdwChecks =
      0x00001000;  // SECURITY_FLAG_IGNORE_CERT_CN_INVALID
  extra_policy_para.pwszServerName = base::as_writable_wcstr(hostname16);

  CERT_CHAIN_POLICY_PARA policy_para;
  memset(&policy_para, 0, sizeof(policy_para));
  policy_para.cbSize = sizeof(policy_para);
  policy_para.dwFlags = 0;
  policy_para.pvExtraPolicyPara = &extra_policy_para;

  CERT_CHAIN_POLICY_STATUS policy_status;
  memset(&policy_status, 0, sizeof(policy_status));
  policy_status.cbSize = sizeof(policy_status);

  if (!CertVerifyCertificateChainPolicy(
           CERT_CHAIN_POLICY_SSL,
           chain_context,
           &policy_para,
           &policy_status)) {
    return MapSecurityError(GetLastError());
  }

  if (policy_status.dwError) {
    verify_result->cert_status |= MapNetErrorToCertStatus(
        MapSecurityError(policy_status.dwError));
  }

  // Mask off revocation checking failures unless hard-fail revocation checking
  // for local anchors is enabled and the chain is issued by a local root.
  // (CheckEV will still check chain_context->TrustStatus.dwErrorStatus directly
  // so as to not mark as EV if revocation information was not available.)
  if (!(!verify_result->is_issued_by_known_root &&
        (flags & VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS))) {
    verify_result->cert_status &= ~(CERT_STATUS_NO_REVOCATION_MECHANISM |
                                    CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  }

  AppendPublicKeyHashesAndUpdateKnownRoot(
      chain_context, &verify_result->public_key_hashes,
      &verify_result->is_issued_by_known_root);

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (ev_policy_oid &&
      CheckEV(chain_context, rev_checking_enabled, ev_policy_oid)) {
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  }

  LogNameNormalizationMetrics(".Win", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net
