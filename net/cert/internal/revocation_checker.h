// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_REVOCATION_CHECKER_H_
#define NET_CERT_INTERNAL_REVOCATION_CHECKER_H_

#include <string_view>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/crl_set.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/ocsp.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

class CertNetFetcher;

// Baseline Requirements 1.6.5, section 4.9.7:
//     For the status of Subscriber Certificates: If the CA publishes a CRL,
//     then the CA SHALL update and reissue CRLs at least once every seven
//     days, and the value of the nextUpdate field MUST NOT be more than ten
//     days beyond the value of the thisUpdate field.
//
// Baseline Requirements 1.6.5, section 4.9.10:
//     For the status of Subscriber Certificates: The CA SHALL update
//     information provided via an Online Certificate Status Protocol at least
//     every four days.  OCSP responses from this service MUST have a maximum
//     expiration time of ten days.
//
// Use 7 days as the max allowable leaf revocation status age, which is
// sufficient for both CRL and OCSP, and which aligns with Microsoft policies.
constexpr base::TimeDelta kMaxRevocationLeafUpdateAge = base::Days(7);

// Baseline Requirements 1.6.5, section 4.9.7:
//     For the status of Subordinate CA Certificates: The CA SHALL update and
//     reissue CRLs at least (i) once every twelve months and (ii) within 24
//     hours after revoking a Subordinate CA Certificate, and the value of the
//     nextUpdate field MUST NOT be more than twelve months beyond the value of
//     the thisUpdate field.
//
// Baseline Requirements 1.6.5, section 4.9.10:
//     For the status of Subordinate CA Certificates: The CA SHALL update
//     information provided via an Online Certificate Status Protocol at least
//     (i) every twelve months and (ii) within 24 hours after revoking a
//     Subordinate CA Certificate.
//
// Use 366 days to allow for leap years, though it is overly permissive in
// other years.
constexpr base::TimeDelta kMaxRevocationIntermediateUpdateAge = base::Days(366);

// RevocationPolicy describes how revocation should be carried out for a
// particular chain.
// Callers should not rely on the default-initialized value, but should fully
// specify all the parameters. The default values specify a strict revocation
// checking mode, in case users fail to fully set the parameters.
struct NET_EXPORT_PRIVATE RevocationPolicy {
  // If |check_revocation| is true, then revocation checking is mandatory. This
  // means that every certificate in the chain (excluding trust anchors) must
  // have valid (unexpired) revocation information proving it to be unrevoked.
  //
  // The mechanisms used for checking revocation may include stapled OCSP,
  // cached OCSP, online OCSP, cached CRL, online CRL.
  //
  // The other properties of RevocationPolicy place further constraints on how
  // revocation checking may proceed.
  bool check_revocation = true;

  // If |networking_allowed| is true then revocation checking is allowed to
  // issue network requests in order to fetch fresh OCSP/CRL. Otherwise
  // networking is not permitted in the course of revocation checking.
  bool networking_allowed = false;

  // If |crl_allowed| is true then CRLs will be checked as a fallback when an
  // OCSP URL is not present or OCSP results are indeterminate.
  bool crl_allowed = true;

  // If set to true, considers certificates lacking URLs for OCSP/CRL to be
  // unrevoked. Otherwise will fail for certificates lacking revocation
  // mechanisms.
  bool allow_missing_info = false;

  // If set to true, other failure to perform revocation checks (e.g. due to a
  // network level failure, OCSP response error status, failure parsing or
  // evaluating the OCSP/CRL response, etc) is considered equivalent to a
  // successful revocation check.
  bool allow_unable_to_check = false;

  // If set to true, enforce requirements specified in the Baseline
  // Requirements such as maximum age of revocation responses.
  bool enforce_baseline_requirements = true;
};

// Checks the revocation status of |certs| according to |policy|, and adds
// any failures to |errors|. On failure errors are added to |errors|. On success
// no errors are added.
//
// |deadline|, if not null, will limit the overall amount of time spent doing
// online revocation checks. If |base::TimeTicks::Now()| exceeds |deadline|, no
// more revocation checks will be attempted. Note that this is not a hard
// limit, the deadline may be exceeded by the individual request timetout of a
// single CertNetFetcher.
//
// |certs| must be a successfully validated chain according to RFC 5280 section
// 6.1, in order from leaf to trust anchor.
//
// |net_fetcher| may be null, however this may lead to failed revocation checks
// depending on |policy|.
//
// |stapled_ocsp_verify_result|, if non-null, will be filled with the result of
// checking the leaf certificate against |stapled_leaf_ocsp_response|.
NET_EXPORT_PRIVATE void CheckValidatedChainRevocation(
    const bssl::ParsedCertificateList& certs,
    const RevocationPolicy& policy,
    base::TimeTicks deadline,
    std::string_view stapled_leaf_ocsp_response,
    base::Time current_time,
    CertNetFetcher* net_fetcher,
    bssl::CertPathErrors* errors,
    bssl::OCSPVerifyResult* stapled_ocsp_verify_result);

// Checks the revocation status of a certificate chain using the CRLSet and adds
// revocation errors to |errors|.
//
// Returns the revocation status of the leaf certificate:
//
// * CRLSet::REVOKED if any certificate in the chain is revoked. Also adds a
//   corresponding error for the certificate in |errors|.
//
// * CRLSet::GOOD if the leaf certificate is covered as GOOD by the CRLSet, and
//   none of the intermediates were revoked according to the CRLSet.
//
// * CRLSet::UNKNOWN if none of the certificates are known to be revoked, and
//   the revocation status of leaf certificate was UNKNOWN by the CRLSet.
NET_EXPORT_PRIVATE CRLSet::Result CheckChainRevocationUsingCRLSet(
    const CRLSet* crl_set,
    const bssl::ParsedCertificateList& certs,
    bssl::CertPathErrors* errors);

}  // namespace net

#endif  // NET_CERT_INTERNAL_REVOCATION_CHECKER_H_
