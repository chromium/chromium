// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_nss.h"

#include <cert.h>
#include <nss.h>
#include <prerror.h>
#include <secerr.h>
#include <sechash.h>
#include <sslerr.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/known_roots.h"
#include "net/cert/known_roots_nss.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/cert_net/nss_ocsp.h"

#include <dlfcn.h>

namespace net {

namespace {

using CacheOCSPResponseFunction = SECStatus (*)(CERTCertDBHandle* handle,
                                                CERTCertificate* cert,
                                                PRTime time,
                                                const SECItem* encodedResponse,
                                                void* pwArg);

typedef std::unique_ptr<
    CERTCertificatePolicies,
    crypto::NSSDestroyer<CERTCertificatePolicies,
                         CERT_DestroyCertificatePoliciesExtension>>
    ScopedCERTCertificatePolicies;

typedef std::unique_ptr<
    CERTCertList,
    crypto::NSSDestroyer<CERTCertList, CERT_DestroyCertList>>
    ScopedCERTCertList;

// ScopedCERTValOutParam manages destruction of values in the CERTValOutParam
// array that cvout points to.  cvout must be initialized as passed to
// CERT_PKIXVerifyCert, so that the array must be terminated with
// cert_po_end type.
// When it goes out of scope, it destroys values of cert_po_trustAnchor
// and cert_po_certList types, but doesn't release the array itself.
class ScopedCERTValOutParam {
 public:
  explicit ScopedCERTValOutParam(CERTValOutParam* cvout) : cvout_(cvout) {}

  ~ScopedCERTValOutParam() {
    Clear();
  }

  // Free the internal resources, but do not release the array itself.
  void Clear() {
    if (cvout_ == NULL)
      return;
    for (CERTValOutParam *p = cvout_; p->type != cert_po_end; p++) {
      switch (p->type) {
        case cert_po_trustAnchor:
          if (p->value.pointer.cert) {
            CERT_DestroyCertificate(p->value.pointer.cert);
            p->value.pointer.cert = NULL;
          }
          break;
        case cert_po_certList:
          if (p->value.pointer.chain) {
            CERT_DestroyCertList(p->value.pointer.chain);
            p->value.pointer.chain = NULL;
          }
          break;
        default:
          break;
      }
    }
  }

 private:
  CERTValOutParam* cvout_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCERTValOutParam);
};

// Map PORT_GetError() return values to our network error codes.
int MapSecurityError(int err) {
  switch (err) {
    case PR_DIRECTORY_LOOKUP_ERROR:  // DNS lookup error.
      return ERR_NAME_NOT_RESOLVED;
    case SEC_ERROR_INVALID_ARGS:
      return ERR_INVALID_ARGUMENT;
    case SSL_ERROR_BAD_CERT_DOMAIN:
      return ERR_CERT_COMMON_NAME_INVALID;
    case SEC_ERROR_INVALID_TIME:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
      return ERR_CERT_DATE_INVALID;
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_BAD_SIGNATURE:
    case SEC_ERROR_APPLICATION_CALLBACK_ERROR:  // Rejected by
                                                // chain_verify_callback.
      return ERR_CERT_AUTHORITY_INVALID;
    // TODO(port): map ERR_CERT_NO_REVOCATION_MECHANISM.
    case SEC_ERROR_OCSP_BAD_HTTP_RESPONSE:
    case SEC_ERROR_OCSP_SERVER_ERROR:
      return ERR_CERT_UNABLE_TO_CHECK_REVOCATION;
    case SEC_ERROR_REVOKED_CERTIFICATE:
    case SEC_ERROR_UNTRUSTED_CERT:  // Treat as revoked.
      return ERR_CERT_REVOKED;
    case SEC_ERROR_CERT_NOT_IN_NAME_SPACE:
      return ERR_CERT_NAME_CONSTRAINT_VIOLATION;
    case SEC_ERROR_BAD_DER:
    case SEC_ERROR_CERT_NOT_VALID:
    // TODO(port): add an ERR_CERT_WRONG_USAGE error code.
    case SEC_ERROR_CERT_USAGES_INVALID:
    case SEC_ERROR_INADEQUATE_KEY_USAGE:  // Key usage.
    case SEC_ERROR_INADEQUATE_CERT_TYPE:  // Extended key usage and whether
                                          // the certificate is a CA.
    case SEC_ERROR_POLICY_VALIDATION_FAILED:
    case SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID:
    case SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION:
    case SEC_ERROR_EXTENSION_VALUE_INVALID:
      return ERR_CERT_INVALID;
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
      return ERR_CERT_WEAK_SIGNATURE_ALGORITHM;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

// Map PORT_GetError() return values to our cert status flags.
CertStatus MapCertErrorToCertStatus(int err) {
  int net_error = MapSecurityError(err);
  return MapNetErrorToCertStatus(net_error);
}

// Extracts the certificate chain from |cert_list| (and optionally |root_cert|)
// into an X509Certificate. If this fails, returns nullptr.
// Note that cert_list[0] is the end entity certificate.
scoped_refptr<X509Certificate> GetCertChainInfo(CERTCertList* cert_list,
                                                CERTCertificate* root_cert) {
  if (!cert_list)
    return nullptr;

  CERTCertificate* verified_cert = NULL;
  std::vector<CERTCertificate*> verified_chain;
  size_t i = 0;
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node), ++i) {
    if (i == 0) {
      verified_cert = node->cert;
    } else {
      // Because of an NSS bug, CERT_PKIXVerifyCert may chain a self-signed
      // certificate of a root CA to another certificate of the same root CA
      // key.  Detect that error and ignore the root CA certificate.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=721288.
      if (node->cert->isRoot) {
        // NOTE: isRoot doesn't mean the certificate is a trust anchor.  It
        // means the certificate is self-signed.  Here we assume isRoot only
        // implies the certificate is self-issued.
        CERTCertListNode* next_node = CERT_LIST_NEXT(node);
        CERTCertificate* next_cert;
        if (!CERT_LIST_END(next_node, cert_list)) {
          next_cert = next_node->cert;
        } else {
          next_cert = root_cert;
        }
        // Test that |node->cert| is actually a self-signed certificate
        // whose key is equal to |next_cert|, and not a self-issued
        // certificate signed by another key of the same CA.
        if (next_cert && SECITEM_ItemsAreEqual(&node->cert->derPublicKey,
                                               &next_cert->derPublicKey)) {
          continue;
        }
      }
      verified_chain.push_back(node->cert);
    }
  }

  if (root_cert)
    verified_chain.push_back(root_cert);

  return x509_util::CreateX509CertificateFromCERTCertificate(verified_cert,
                                                             verified_chain);
}

// Returns true if the given certificate is one of the additional trust anchors.
bool IsAdditionalTrustAnchor(CERTCertList* additional_trust_anchors,
                             CERTCertificate* root) {
  if (!additional_trust_anchors || !root)
    return false;
  for (CERTCertListNode* node = CERT_LIST_HEAD(additional_trust_anchors);
       !CERT_LIST_END(node, additional_trust_anchors);
       node = CERT_LIST_NEXT(node)) {
    if (CERT_CompareCerts(node->cert, root))
      return true;
  }
  return false;
}

enum CRLSetResult {
  kCRLSetOk,
  kCRLSetRevoked,
  kCRLSetUnknown,
};

// CheckRevocationWithCRLSet attempts to check each element of |cert_list|
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
CRLSetResult CheckRevocationWithCRLSet(const CERTCertList* cert_list,
                                       CERTCertificate* root,
                                       CRLSet* crl_set) {
  std::vector<CERTCertificate*> certs;

  if (cert_list) {
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      certs.push_back(node->cert);
    }
  }
  if (root)
    certs.push_back(root);

  // Set to true if any errors are found, which will cause such chains to not be
  // treated as covered by the CRLSet.
  bool error = false;
  // Set to the coverage state of the previous certificate.  As the certificates
  // are iterated over from root to leaf, at the end of the iteration, this
  // indicates the coverage state of the leaf certificate.
  bool last_covered = false;

  // We iterate from the root certificate down to the leaf, keeping track of
  // the issuer's SPKI at each step.
  std::string issuer_spki_hash;
  for (auto i = certs.rbegin(); i != certs.rend(); ++i) {
    CERTCertificate* cert = *i;

    base::StringPiece der(reinterpret_cast<char*>(cert->derCert.data),
                          cert->derCert.len);

    base::StringPiece spki, subject;
    if (!asn1::ExtractSPKIFromDERCert(der, &spki) ||
        !asn1::ExtractSubjectFromDERCert(der, &subject)) {
      NOTREACHED();
      error = true;
      continue;
    }
    const std::string spki_hash = crypto::SHA256HashString(spki);

    base::StringPiece serial_number = base::StringPiece(
        reinterpret_cast<char*>(cert->serialNumber.data),
        cert->serialNumber.len);

    CRLSet::Result result = crl_set->CheckSPKI(spki_hash);

    if (result != CRLSet::REVOKED)
      result = crl_set->CheckSubject(subject, spki_hash);
    if (result != CRLSet::REVOKED && !issuer_spki_hash.empty())
      result = crl_set->CheckSerial(serial_number, issuer_spki_hash);

    issuer_spki_hash = spki_hash;

    switch (result) {
      case CRLSet::REVOKED:
        return kCRLSetRevoked;
      case CRLSet::UNKNOWN:
        last_covered = false;
        continue;
      case CRLSet::GOOD:
        last_covered = true;
        continue;
      default:
        NOTREACHED();
        error = true;
        continue;
    }
  }

  if (error || !last_covered || crl_set->IsExpired())
    return kCRLSetUnknown;
  return kCRLSetOk;
}

// Arguments for CheckChainRevocationWithCRLSet that are curried within the
// CERTChainVerifyCallback's isChainValidArg.
struct CheckChainRevocationArgs {
  // The CRLSet to evaluate against.
  CRLSet* crl_set = nullptr;

  ScopedCERTCertList chain;

  // The next callback to invoke, if the CRLSet does not report any errors.
  CERTChainVerifyCallback* next_callback = nullptr;

  // Indicates that the application callback failure was due to a CRLSet
  // revocation, rather than due to |next_callback| rejecting it. This is
  // used to map the error back to the proper caller-visible error code.
  bool was_revoked = false;
};

SECStatus CheckChainRevocationWithCRLSet(void* is_chain_valid_arg,
                                         const CERTCertList* current_chain,
                                         PRBool* chain_ok) {
  CHECK(is_chain_valid_arg);

  CheckChainRevocationArgs* args =
      static_cast<CheckChainRevocationArgs*>(is_chain_valid_arg);

  args->was_revoked = false;
  args->chain.reset();

  CRLSetResult crlset_result = kCRLSetUnknown;
  if (args->crl_set) {
    crlset_result =
        CheckRevocationWithCRLSet(current_chain, nullptr, args->crl_set);
  }

  if (crlset_result == kCRLSetRevoked) {
    // Record the current chain; as an application callback, libpkix will try
    // to build a better chain, if possible, or otherwise unwind the path graph
    // and forget that it found a potentially-valid, but application-rejected
    // chain. For ease with later functions, this is implemented by duplicating
    // the CERTCertList, which will take ownership of the certs inside it.
    args->chain.reset(CERT_NewCertList());
    for (CERTCertListNode* node = CERT_LIST_HEAD(current_chain);
         !CERT_LIST_END(node, current_chain); node = CERT_LIST_NEXT(node)) {
      if (CERT_AddCertToListTail(args->chain.get(),
                                 CERT_DupCertificate(node->cert)) !=
          SECSuccess) {
        args->chain.reset();
        break;
      }
    }
    args->was_revoked = true;
    *chain_ok = PR_FALSE;
    return SECSuccess;
  }

  *chain_ok = PR_TRUE;
  if (!args->next_callback || !args->next_callback->isChainValid)
    return SECSuccess;

  return (*args->next_callback->isChainValid)(
      args->next_callback->isChainValidArg, current_chain, chain_ok);
}

// Forward declarations.
SECStatus RetryPKIXVerifyCertWithWorkarounds(CERTCertificate* cert_handle,
                                             int num_policy_oids,
                                             std::vector<CERTValInParam>* cvin,
                                             CERTValOutParam* cvout);
SECOidTag GetFirstCertPolicy(CERTCertificate* cert_handle);

// Call CERT_PKIXVerifyCert for the cert_handle.
// Verification results are stored in an array of CERTValOutParam.
// If |hard_fail| is true, and no policy_oids are supplied (eg: EV is NOT being
// checked), then the failure to obtain valid CRL/OCSP information for all
// certificates that contain CRL/OCSP URLs will cause the certificate to be
// treated as if it was revoked. Since failures may be caused by transient
// network failures or by malicious attackers, in general, hard_fail should be
// false.
// If policy_oids is not NULL and num_policy_oids is positive, policies
// are also checked.
// additional_trust_anchors is an optional list of certificates that can be
// trusted as anchors when building a certificate chain.
// Caller must initialize cvout before calling this function.
SECStatus PKIXVerifyCert(CERTCertificate* cert_handle,
                         bool check_revocation,
                         bool hard_fail,
                         const SECOidTag* policy_oids,
                         int num_policy_oids,
                         CERTCertList* additional_trust_anchors,
                         CERTChainVerifyCallback* chain_verify_callback,
                         CERTValOutParam* cvout) {
  bool use_crl = check_revocation;
  bool use_ocsp = check_revocation;

  PRUint64 revocation_method_flags =
      CERT_REV_M_DO_NOT_TEST_USING_THIS_METHOD |
      CERT_REV_M_ALLOW_NETWORK_FETCHING |
      CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE |
      CERT_REV_M_IGNORE_MISSING_FRESH_INFO |
      CERT_REV_M_STOP_TESTING_ON_FRESH_INFO;
  PRUint64 revocation_method_independent_flags =
      CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST;
  if (check_revocation && policy_oids && num_policy_oids > 0) {
    // EV verification requires revocation checking.  Consider the certificate
    // revoked if we don't have revocation info.
    // TODO(wtc): Add a bool parameter to expressly specify we're doing EV
    // verification or we want strict revocation flags.
    revocation_method_flags |= CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE;
    revocation_method_independent_flags |=
        CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE;
  } else if (check_revocation && hard_fail) {
    revocation_method_flags |= CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO;
    revocation_method_independent_flags |=
        CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE;
  } else {
    revocation_method_flags |= CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE;
    revocation_method_independent_flags |=
        CERT_REV_MI_NO_OVERALL_INFO_REQUIREMENT;
  }
  PRUint64 method_flags[2];
  method_flags[cert_revocation_method_crl] = revocation_method_flags;
  method_flags[cert_revocation_method_ocsp] = revocation_method_flags;

  if (use_crl) {
    method_flags[cert_revocation_method_crl] |=
        CERT_REV_M_TEST_USING_THIS_METHOD;
  }
  if (use_ocsp) {
    method_flags[cert_revocation_method_ocsp] |=
        CERT_REV_M_TEST_USING_THIS_METHOD;
  }

  CERTRevocationMethodIndex preferred_revocation_methods[1];
  if (use_ocsp) {
    preferred_revocation_methods[0] = cert_revocation_method_ocsp;
  } else {
    preferred_revocation_methods[0] = cert_revocation_method_crl;
  }

  CERTRevocationFlags revocation_flags;
  revocation_flags.leafTests.number_of_defined_methods =
      base::size(method_flags);
  revocation_flags.leafTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.leafTests.number_of_preferred_methods =
      base::size(preferred_revocation_methods);
  revocation_flags.leafTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.leafTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;

  revocation_flags.chainTests.number_of_defined_methods =
      base::size(method_flags);
  revocation_flags.chainTests.cert_rev_flags_per_method = method_flags;
  revocation_flags.chainTests.number_of_preferred_methods =
      base::size(preferred_revocation_methods);
  revocation_flags.chainTests.preferred_methods = preferred_revocation_methods;
  revocation_flags.chainTests.cert_rev_method_independent_flags =
      revocation_method_independent_flags;


  std::vector<CERTValInParam> cvin;
  cvin.reserve(7);
  CERTValInParam in_param;
  in_param.type = cert_pi_revocationFlags;
  in_param.value.pointer.revocation = &revocation_flags;
  cvin.push_back(in_param);
  if (policy_oids && num_policy_oids > 0) {
    in_param.type = cert_pi_policyOID;
    in_param.value.arraySize = num_policy_oids;
    in_param.value.array.oids = policy_oids;
    cvin.push_back(in_param);
  }
  if (additional_trust_anchors) {
    in_param.type = cert_pi_trustAnchors;
    in_param.value.pointer.chain = additional_trust_anchors;
    cvin.push_back(in_param);
    in_param.type = cert_pi_useOnlyTrustAnchors;
    in_param.value.scalar.b = PR_FALSE;
    cvin.push_back(in_param);
  }
  if (chain_verify_callback) {
    in_param.type = cert_pi_chainVerifyCallback;
    in_param.value.pointer.chainVerifyCallback = chain_verify_callback;
    cvin.push_back(in_param);
  }
  in_param.type = cert_pi_end;
  cvin.push_back(in_param);

  SECStatus rv = CERT_PKIXVerifyCert(cert_handle, certificateUsageSSLServer,
                                     cvin.data(), cvout, nullptr);
  if (rv != SECSuccess) {
    rv = RetryPKIXVerifyCertWithWorkarounds(cert_handle, num_policy_oids, &cvin,
                                            cvout);
  }
  return rv;
}

// PKIXVerifyCert calls this function to work around some bugs in
// CERT_PKIXVerifyCert.  All the arguments of this function are either the
// arguments or local variables of PKIXVerifyCert.
SECStatus RetryPKIXVerifyCertWithWorkarounds(CERTCertificate* cert_handle,
                                             int num_policy_oids,
                                             std::vector<CERTValInParam>* cvin,
                                             CERTValOutParam* cvout) {
  // We call this function when the first CERT_PKIXVerifyCert call in
  // PKIXVerifyCert failed,  so we initialize |rv| to SECFailure.
  SECStatus rv = SECFailure;
  int nss_error = PORT_GetError();
  CERTValInParam in_param;

  // If we get SEC_ERROR_UNKNOWN_ISSUER, we may be missing an intermediate
  // CA certificate, so we retry with cert_pi_useAIACertFetch.
  // cert_pi_useAIACertFetch has several bugs in its error handling and
  // error reporting (NSS bug 528743), so we don't use it by default.
  // Note: When building a certificate chain, CERT_PKIXVerifyCert may
  // incorrectly pick a CA certificate with the same subject name as the
  // missing intermediate CA certificate, and  fail with the
  // SEC_ERROR_BAD_SIGNATURE error (NSS bug 524013), so we also retry with
  // cert_pi_useAIACertFetch on SEC_ERROR_BAD_SIGNATURE.
  if ((nss_error == SEC_ERROR_UNKNOWN_ISSUER ||
       nss_error == SEC_ERROR_BAD_SIGNATURE)) {
    DCHECK_EQ(cvin->back().type,  cert_pi_end);
    cvin->pop_back();
    in_param.type = cert_pi_useAIACertFetch;
    in_param.value.scalar.b = PR_TRUE;
    cvin->push_back(in_param);
    in_param.type = cert_pi_end;
    cvin->push_back(in_param);
    rv = CERT_PKIXVerifyCert(cert_handle, certificateUsageSSLServer,
                             &(*cvin)[0], cvout, NULL);
    if (rv == SECSuccess)
      return rv;
    int new_nss_error = PORT_GetError();
    if (new_nss_error == SEC_ERROR_INVALID_ARGS ||
        new_nss_error == SEC_ERROR_UNKNOWN_AIA_LOCATION_TYPE ||
        new_nss_error == SEC_ERROR_BAD_INFO_ACCESS_LOCATION ||
        new_nss_error == SEC_ERROR_BAD_HTTP_RESPONSE ||
        new_nss_error == SEC_ERROR_BAD_LDAP_RESPONSE ||
        !IS_SEC_ERROR(new_nss_error)) {
      // Use the original error code because of cert_pi_useAIACertFetch's
      // bad error reporting.
      PORT_SetError(nss_error);
      return rv;
    }
    nss_error = new_nss_error;
  }

  // If an intermediate CA certificate has requireExplicitPolicy in its
  // policyConstraints extension, CERT_PKIXVerifyCert fails with
  // SEC_ERROR_POLICY_VALIDATION_FAILED because we didn't specify any
  // certificate policy (NSS bug 552775).  So we retry with the certificate
  // policy found in the server certificate.
  if (nss_error == SEC_ERROR_POLICY_VALIDATION_FAILED &&
      num_policy_oids == 0) {
    SECOidTag policy = GetFirstCertPolicy(cert_handle);
    if (policy != SEC_OID_UNKNOWN) {
      DCHECK_EQ(cvin->back().type,  cert_pi_end);
      cvin->pop_back();
      in_param.type = cert_pi_policyOID;
      in_param.value.arraySize = 1;
      in_param.value.array.oids = &policy;
      cvin->push_back(in_param);
      in_param.type = cert_pi_end;
      cvin->push_back(in_param);
      rv = CERT_PKIXVerifyCert(cert_handle, certificateUsageSSLServer,
                               &(*cvin)[0], cvout, NULL);
      if (rv != SECSuccess) {
        // Use the original error code.
        PORT_SetError(nss_error);
      }
    }
  }

  return rv;
}

// Decodes the certificatePolicies extension of the certificate.  Returns
// NULL if the certificate doesn't have the extension or the extension can't
// be decoded.  The returned value must be freed with a
// CERT_DestroyCertificatePoliciesExtension call.
CERTCertificatePolicies* DecodeCertPolicies(
    CERTCertificate* cert_handle) {
  SECItem policy_ext;
  SECStatus rv = CERT_FindCertExtension(cert_handle,
                                        SEC_OID_X509_CERTIFICATE_POLICIES,
                                        &policy_ext);
  if (rv != SECSuccess)
    return NULL;
  CERTCertificatePolicies* policies =
      CERT_DecodeCertificatePoliciesExtension(&policy_ext);
  SECITEM_FreeItem(&policy_ext, PR_FALSE);
  return policies;
}

// Returns the OID tag for the first certificate policy in the certificate's
// certificatePolicies extension.  Returns SEC_OID_UNKNOWN if the certificate
// has no certificate policy.
SECOidTag GetFirstCertPolicy(CERTCertificate* cert_handle) {
  ScopedCERTCertificatePolicies policies(DecodeCertPolicies(cert_handle));
  if (!policies.get())
    return SEC_OID_UNKNOWN;

  CERTPolicyInfo* policy_info = policies->policyInfos[0];
  if (!policy_info)
    return SEC_OID_UNKNOWN;
  if (policy_info->oid != SEC_OID_UNKNOWN)
    return policy_info->oid;

  // The certificate policy is unknown to NSS.  We need to create a dynamic
  // OID tag for the policy.
  SECOidData od;
  od.oid.len = policy_info->policyID.len;
  od.oid.data = policy_info->policyID.data;
  od.offset = SEC_OID_UNKNOWN;
  // NSS doesn't allow us to pass an empty description, so I use a hardcoded,
  // default description here.  The description doesn't need to be unique for
  // each OID.
  od.desc = "a certificate policy";
  od.mechanism = CKM_INVALID_MECHANISM;
  od.supportedExtension = INVALID_CERT_EXTENSION;
  return SECOID_AddEntry(&od);
}

HashValue CertPublicKeyHashSHA256(CERTCertificate* cert) {
  HashValue hash(HASH_VALUE_SHA256);
  SECStatus rv = HASH_HashBuf(HASH_AlgSHA256, hash.data(),
                              cert->derPublicKey.data, cert->derPublicKey.len);
  DCHECK_EQ(rv, SECSuccess);
  return hash;
}

void AppendPublicKeyHashesAndTestKnownRoot(CERTCertList* cert_list,
                                           CERTCertificate* root_cert,
                                           HashValueVector* hashes,
                                           bool* known_root) {
  *known_root = false;

  if (!cert_list)
    return;

  // First, traverse the list to build the list of public key hashes, in order
  // of leaf to root.
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    hashes->push_back(CertPublicKeyHashSHA256(node->cert));
  }
  if (root_cert) {
    hashes->push_back(CertPublicKeyHashSHA256(root_cert));
  }

  // Second, as an optimization, work from the hashes from the last (presumed
  // root) to the leaf, checking against the built-in list.
  for (auto it = hashes->rbegin(); it != hashes->rend() && !*known_root; ++it) {
    *known_root = GetNetTrustAnchorHistogramIdForSPKI(*it) != 0;
  }

  // Third, see if a root_cert was provided, and if so, if it matches a
  // built-in root (it should, if provided).
  if (root_cert && !*known_root) {
    *known_root = IsKnownRoot(root_cert);
  }

  // Finally, if all else has failed and nothing short-circuited, walk the
  // remainder of the chain. As it's unlikely to reach this point, this just
  // walks from the leaf and is not optimized, favoring readability.
  for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
       !*known_root && !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    *known_root = IsKnownRoot(node->cert);
  }
}

// Returns true if |cert_handle| contains a policy OID that is an EV policy
// OID according to |metadata|, storing the resulting policy OID in
// |*ev_policy_oid|. A true return is not sufficient to establish that a
// certificate is EV, but a false return is sufficient to establish the
// certificate cannot be EV.
bool IsEVCandidate(EVRootCAMetadata* metadata,
                   CERTCertificate* cert_handle,
                   SECOidTag* ev_policy_oid) {
  *ev_policy_oid = SEC_OID_UNKNOWN;
  DCHECK(cert_handle);
  ScopedCERTCertificatePolicies policies(DecodeCertPolicies(cert_handle));
  if (!policies.get())
    return false;

  CERTPolicyInfo** policy_infos = policies->policyInfos;
  while (*policy_infos != NULL) {
    CERTPolicyInfo* policy_info = *policy_infos++;
    // If the Policy OID is unknown, that implicitly means it has not been
    // registered as an EV policy.
    if (policy_info->oid == SEC_OID_UNKNOWN)
      continue;
    if (metadata->IsEVPolicyOID(policy_info->oid)) {
      *ev_policy_oid = policy_info->oid;

      // De-prioritize the CA/Browser forum Extended Validation policy
      // (2.23.140.1.1). See crbug.com/705285.
      if (!EVRootCAMetadata::IsCaBrowserForumEvOid(policy_info->oid))
        break;
    }
  }

  return *ev_policy_oid != SEC_OID_UNKNOWN;
}

// Studied Mozilla's code (esp. security/manager/ssl/src/nsIdentityChecking.cpp
// and nsNSSCertHelper.cpp) to learn how to verify EV certificate.
// TODO(wtc): A possible optimization is that we get the trust anchor from
// the first PKIXVerifyCert call.  We look up the EV policy for the trust
// anchor.  If the trust anchor has no EV policy, we know the cert isn't EV.
// Otherwise, we pass just that EV policy (as opposed to all the EV policies)
// to the second PKIXVerifyCert call.
bool VerifyEV(CERTCertificate* cert_handle,
              int flags,
              CRLSet* crl_set,
              bool rev_checking_enabled,
              EVRootCAMetadata* metadata,
              SECOidTag ev_policy_oid,
              CERTCertList* additional_trust_anchors,
              CERTChainVerifyCallback* chain_verify_callback) {
  CERTValOutParam cvout[3];
  int cvout_index = 0;
  cvout[cvout_index].type = cert_po_certList;
  cvout[cvout_index].value.pointer.chain = NULL;
  int cvout_cert_list_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_trustAnchor;
  cvout[cvout_index].value.pointer.cert = NULL;
  int cvout_trust_anchor_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_end;
  ScopedCERTValOutParam scoped_cvout(cvout);

  SECStatus status = PKIXVerifyCert(
      cert_handle,
      rev_checking_enabled,
      true, /* hard fail is implied in EV. */
      &ev_policy_oid,
      1,
      additional_trust_anchors,
      chain_verify_callback,
      cvout);
  if (status != SECSuccess)
    return false;

  CERTCertificate* root_ca =
      cvout[cvout_trust_anchor_index].value.pointer.cert;
  if (root_ca == NULL)
    return false;

  // This second PKIXVerifyCert call could have found a different certification
  // path and one or more of the certificates on this new path, that weren't on
  // the old path, might have been revoked.
  CRLSetResult crl_set_result = CheckRevocationWithCRLSet(
      cvout[cvout_cert_list_index].value.pointer.chain,
      cvout[cvout_trust_anchor_index].value.pointer.cert, crl_set);
  if (crl_set_result == kCRLSetRevoked)
    return false;

  SHA256HashValue fingerprint;
  crypto::SHA256HashString(
      base::StringPiece(reinterpret_cast<const char*>(root_ca->derCert.data),
                        root_ca->derCert.len),
      fingerprint.data, sizeof(fingerprint.data));
  return metadata->HasEVPolicyOID(fingerprint, ev_policy_oid);
}

// Convert a CertificateList to an NSS CERTCertList. If any certs couldn't be
// converted, they are silently ignored.
ScopedCERTCertList CertificateListToCERTCertListIgnoringErrors(
    const CertificateList& list) {
  ScopedCERTCertList result(CERT_NewCertList());
  for (size_t i = 0; i < list.size(); ++i) {
    ScopedCERTCertificate cert =
        x509_util::CreateCERTCertificateFromX509Certificate(list[i].get());
    if (cert)
      CERT_AddCertToListTail(result.get(), cert.release());
    else
      LOG(WARNING) << "ignoring cert: " << list[i]->subject().GetDisplayName();
  }
  return result;
}

}  // namespace

CertVerifyProcNSS::CertVerifyProcNSS() = default;

CertVerifyProcNSS::~CertVerifyProcNSS() = default;

bool CertVerifyProcNSS::SupportsAdditionalTrustAnchors() const {
  return true;
}

NO_SANITIZE("cfi-icall")
int CertVerifyProcNSS::VerifyInternalImpl(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CERTChainVerifyCallback* chain_verify_callback,
    CertVerifyResult* verify_result) {
  crypto::EnsureNSSInit();
  EnsureNSSHttpIOInit();

  // Convert the whole input chain into NSS certificates. Even though only the
  // target cert is explicitly referred to in this function, creating NSS
  // certificates for the intermediates is required for PKIXVerifyCert to find
  // them during chain building.
  ScopedCERTCertificateList input_chain =
      x509_util::CreateCERTCertificateListFromX509Certificate(
          cert, x509_util::InvalidIntermediateBehavior::kIgnore);
  if (input_chain.empty()) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }
  CERTCertificate* cert_handle = input_chain[0].get();

  static CacheOCSPResponseFunction cache_ocsp_response_from_side_channel =
      reinterpret_cast<CacheOCSPResponseFunction>(
          dlsym(RTLD_DEFAULT, "CERT_CacheOCSPResponseFromSideChannel"));
  if (!ocsp_response.empty() && cache_ocsp_response_from_side_channel) {
    // Note: NSS uses a thread-safe global hash table, so this call will
    // affect any concurrent verification operations on |cert| or copies of
    // the same certificate. This is an unavoidable limitation of NSS's OCSP
    // API.
    SECItem ocsp_response_item;
    ocsp_response_item.data = reinterpret_cast<unsigned char*>(
        const_cast<char*>(ocsp_response.data()));
    ocsp_response_item.len = ocsp_response.size();
    cache_ocsp_response_from_side_channel(CERT_GetDefaultCertDB(), cert_handle,
                                          PR_Now(), &ocsp_response_item,
                                          nullptr);
  }

  // Setup a callback to call into CheckChainRevocationWithCRLSet with the
  // current CRLSet. If the CRLSet revokes a given chain, |was_revoked|
  // will be set to true.
  // The same callback and args are used for every invocation of
  // PKIXVerifyCert, as CheckChainRevocationWithCRLSet handles resetting
  // |was_revoked| as necessary.
  CheckChainRevocationArgs check_chain_revocation_args;
  check_chain_revocation_args.crl_set = crl_set;
  check_chain_revocation_args.next_callback = chain_verify_callback;

  CERTChainVerifyCallback crlset_callback;
  memset(&crlset_callback, 0, sizeof(crlset_callback));
  crlset_callback.isChainValid = &CheckChainRevocationWithCRLSet;
  crlset_callback.isChainValidArg =
      static_cast<void*>(&check_chain_revocation_args);

  // Make sure that the cert is valid now.
  SECCertTimeValidity validity = CERT_CheckCertValidTimes(
      cert_handle, PR_Now(), PR_TRUE);
  if (validity != secCertTimeValid)
    verify_result->cert_status |= CERT_STATUS_DATE_INVALID;

  CERTValOutParam cvout[3];
  int cvout_index = 0;
  cvout[cvout_index].type = cert_po_certList;
  cvout[cvout_index].value.pointer.chain = NULL;
  int cvout_cert_list_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_trustAnchor;
  cvout[cvout_index].value.pointer.cert = NULL;
  int cvout_trust_anchor_index = cvout_index;
  cvout_index++;
  cvout[cvout_index].type = cert_po_end;
  ScopedCERTValOutParam scoped_cvout(cvout);

  EVRootCAMetadata* metadata = EVRootCAMetadata::GetInstance();
  SECOidTag ev_policy_oid = SEC_OID_UNKNOWN;
  bool is_ev_candidate =
      IsEVCandidate(metadata, cert_handle, &ev_policy_oid);
  bool check_revocation = (flags & VERIFY_REV_CHECKING_ENABLED);
  if (check_revocation)
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

  ScopedCERTCertList trust_anchors;
  if (!additional_trust_anchors.empty()) {
    trust_anchors =
        CertificateListToCERTCertListIgnoringErrors(additional_trust_anchors);
  }

  SECStatus status =
      PKIXVerifyCert(cert_handle, check_revocation, false, NULL, 0,
                     trust_anchors.get(), &crlset_callback, cvout);

  bool known_root = false;
  HashValueVector hashes;
  if (status == SECSuccess) {
    AppendPublicKeyHashesAndTestKnownRoot(
        cvout[cvout_cert_list_index].value.pointer.chain,
        cvout[cvout_trust_anchor_index].value.pointer.cert, &hashes,
        &known_root);
  } else if (status == SECFailure &&
             PORT_GetError() == SEC_ERROR_APPLICATION_CALLBACK_ERROR &&
             check_chain_revocation_args.was_revoked) {
    AppendPublicKeyHashesAndTestKnownRoot(
        check_chain_revocation_args.chain.get(), nullptr, &hashes, &known_root);
    // Restore the error (which may have been erased).
    PORT_SetError(SEC_ERROR_APPLICATION_CALLBACK_ERROR);
  }

  if (status == SECSuccess &&
      (flags & VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS) && !known_root) {
    // TODO(rsleevi): Optimize this by supplying the constructed chain to
    // libpkix via cvin. Omitting for now, due to lack of coverage in upstream
    // NSS tests for that feature.
    scoped_cvout.Clear();
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
    status = PKIXVerifyCert(cert_handle, true, true, NULL, 0,
                            trust_anchors.get(), &crlset_callback, cvout);
    if (status == SECSuccess) {
      AppendPublicKeyHashesAndTestKnownRoot(
          cvout[cvout_cert_list_index].value.pointer.chain,
          cvout[cvout_trust_anchor_index].value.pointer.cert, &hashes,
          &known_root);
    } else if (status == SECFailure &&
               PORT_GetError() == SEC_ERROR_APPLICATION_CALLBACK_ERROR &&
               check_chain_revocation_args.was_revoked) {
      AppendPublicKeyHashesAndTestKnownRoot(
          check_chain_revocation_args.chain.get(), nullptr, &hashes,
          &known_root);
      // Restore the error (which may have been erased).
      PORT_SetError(SEC_ERROR_APPLICATION_CALLBACK_ERROR);
    }
  }

  if (status == SECSuccess ||
      (status == SECFailure &&
       PORT_GetError() == SEC_ERROR_APPLICATION_CALLBACK_ERROR &&
       check_chain_revocation_args.was_revoked)) {
    verify_result->public_key_hashes = hashes;
    verify_result->is_issued_by_known_root = known_root;

    if (status == SECFailure) {
      verify_result->verified_cert =
          GetCertChainInfo(check_chain_revocation_args.chain.get(), nullptr);
      // Restore the error (which may have been erased).
      PORT_SetError(SEC_ERROR_APPLICATION_CALLBACK_ERROR);
    } else {
      verify_result->verified_cert =
          GetCertChainInfo(cvout[cvout_cert_list_index].value.pointer.chain,
                           cvout[cvout_trust_anchor_index].value.pointer.cert);
      verify_result->is_issued_by_additional_trust_anchor =
          IsAdditionalTrustAnchor(
              trust_anchors.get(),
              cvout[cvout_trust_anchor_index].value.pointer.cert);
    }
    if (!verify_result->verified_cert)
      verify_result->cert_status |= CERT_STATUS_INVALID;
  }

  CRLSetResult crl_set_result = kCRLSetUnknown;
  if (status == SECSuccess) {
    // Reverify the returned chain; NSS should have already called
    // CheckChainRevocationWithCRLSet prior to returning, but given the
    // edge cases (self-signed certs that are trusted; cached chains;
    // unreadable code), this is more about defense in depth than
    // functional necessity.
    crl_set_result = CheckRevocationWithCRLSet(
        cvout[cvout_cert_list_index].value.pointer.chain,
        cvout[cvout_trust_anchor_index].value.pointer.cert, crl_set);
    if (crl_set_result == kCRLSetRevoked) {
      PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
      status = SECFailure;
    }
  } else if (PORT_GetError() == SEC_ERROR_APPLICATION_CALLBACK_ERROR &&
             check_chain_revocation_args.was_revoked) {
    // If a CRLSet was supplied, and the error was an application callback
    // error, then it was directed through the CRLSet code and that
    // particular chain was revoked.
    PORT_SetError(SEC_ERROR_REVOKED_CERTIFICATE);
  }

  if (status != SECSuccess) {
    int err = PORT_GetError();
    LOG(ERROR) << "CERT_PKIXVerifyCert for " << hostname
               << " failed err=" << err;
    // CERT_PKIXVerifyCert rerports the wrong error code for
    // expired certificates (NSS bug 491174)
    if (err == SEC_ERROR_CERT_NOT_VALID &&
        (verify_result->cert_status & CERT_STATUS_DATE_INVALID))
      err = SEC_ERROR_EXPIRED_CERTIFICATE;
    CertStatus cert_status = MapCertErrorToCertStatus(err);
    if (cert_status) {
      verify_result->cert_status |= cert_status;
      return MapCertStatusToNetError(verify_result->cert_status);
    }
    // |err| is not a certificate error.
    return MapSecurityError(err);
  }

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (is_ev_candidate) {
    check_revocation |= crl_set_result != kCRLSetOk;
    if (check_revocation)
      verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

    // TODO(mattm): This is weird, VerifyEV might verify a different path but
    // the non-EV path is what actually gets returned just with the EV bit
    // added.
    if (VerifyEV(cert_handle, flags, crl_set, check_revocation, metadata,
                 ev_policy_oid, trust_anchors.get(), &crlset_callback)) {
      verify_result->cert_status |= CERT_STATUS_IS_EV;
    }
  }

  LogNameNormalizationMetrics(".NSS", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

int CertVerifyProcNSS::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  return VerifyInternalImpl(cert, hostname, ocsp_response, flags, crl_set,
                            additional_trust_anchors,
                            NULL,  // chain_verify_callback
                            verify_result);
}

}  // namespace net
