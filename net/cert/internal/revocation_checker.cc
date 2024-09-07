// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/revocation_checker.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/logging.h"
#include "crypto/sha2.h"
#include "net/cert/cert_net_fetcher.h"
#include "third_party/boringssl/src/pki/common_cert_errors.h"
#include "third_party/boringssl/src/pki/crl.h"
#include "third_party/boringssl/src/pki/ocsp.h"
#include "third_party/boringssl/src/pki/ocsp_verify_result.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store.h"
#include "url/gurl.h"

namespace net {

namespace {

void MarkCertificateRevoked(bssl::CertErrors* errors) {
  // TODO(eroman): Add a parameter to the error indicating which mechanism
  // caused the revocation (i.e. CRLSet, OCSP, stapled OCSP, etc).
  errors->AddError(bssl::cert_errors::kCertificateRevoked);
}

// Checks the revocation status of |certs[target_cert_index]| according to
// |policy|. If the checks failed, returns false and adds errors to
// |cert_errors|.
//
// TODO(eroman): Make the verification time an input.
bool CheckCertRevocation(const bssl::ParsedCertificateList& certs,
                         size_t target_cert_index,
                         const RevocationPolicy& policy,
                         base::TimeTicks deadline,
                         std::string_view stapled_ocsp_response,
                         std::optional<int64_t> max_age_seconds,
                         base::Time current_time,
                         CertNetFetcher* net_fetcher,
                         bssl::CertErrors* cert_errors,
                         bssl::OCSPVerifyResult* stapled_ocsp_verify_result) {
  DCHECK_LT(target_cert_index, certs.size());
  const bssl::ParsedCertificate* cert = certs[target_cert_index].get();
  const bssl::ParsedCertificate* issuer_cert =
      target_cert_index + 1 < certs.size() ? certs[target_cert_index + 1].get()
                                           : nullptr;

  time_t time_now = current_time.ToTimeT();

  // Check using stapled OCSP, if available.
  if (!stapled_ocsp_response.empty() && issuer_cert) {
    bssl::OCSPVerifyResult::ResponseStatus response_details;
    bssl::OCSPRevocationStatus ocsp_status =
        bssl::CheckOCSP(stapled_ocsp_response, cert, issuer_cert, time_now,
                        max_age_seconds, &response_details);
    if (stapled_ocsp_verify_result) {
      stapled_ocsp_verify_result->response_status = response_details;
      stapled_ocsp_verify_result->revocation_status = ocsp_status;
    }

    // TODO(eroman): Save the stapled OCSP response to cache.
    switch (ocsp_status) {
      case bssl::OCSPRevocationStatus::REVOKED:
        MarkCertificateRevoked(cert_errors);
        return false;
      case bssl::OCSPRevocationStatus::GOOD:
        return true;
      case bssl::OCSPRevocationStatus::UNKNOWN:
        // TODO(eroman): If the OCSP response was invalid, should we keep
        //               looking or fail?
        break;
    }
  }

  if (!policy.check_revocation) {
    // TODO(eroman): Should still check CRL/OCSP caches.
    return true;
  }

  bool found_revocation_info = false;

  // Check OCSP.
  if (cert->has_authority_info_access()) {
    // Try each of the OCSP URIs
    for (const auto& ocsp_uri : cert->ocsp_uris()) {
      // Only consider http:// URLs (https:// could create a circular
      // dependency).
      GURL parsed_ocsp_url(ocsp_uri);
      if (!parsed_ocsp_url.is_valid() ||
          !parsed_ocsp_url.SchemeIs(url::kHttpScheme)) {
        continue;
      }

      found_revocation_info = true;

      // Check the deadline after setting found_revocation_info, to not give a
      // misleading kNoRevocationMechanism failure.
      if (!deadline.is_null() && base::TimeTicks::Now() > deadline)
        break;

      if (!policy.networking_allowed)
        continue;

      if (!net_fetcher) {
        LOG(ERROR) << "Cannot fetch OCSP as didn't specify a |net_fetcher|";
        continue;
      }

      // TODO(eroman): Duplication of work if there are multiple URLs to try.
      // TODO(eroman): Are there cases where we would need to POST instead?
      std::optional<std::string> get_url_str =
          CreateOCSPGetURL(cert, issuer_cert, ocsp_uri);
      if (!get_url_str.has_value()) {
        // An unexpected failure from BoringSSL, or the input was too large to
        // base64-encode.
        continue;
      }
      GURL get_url(get_url_str.value());
      if (!get_url.is_valid()) {
        // Invalid URL.
        continue;
      }

      // Fetch it over network.
      //
      // TODO(eroman): Issue POST instead of GET if request is larger than 255
      //               bytes?
      // TODO(eroman): Improve interplay with HTTP cache.
      std::unique_ptr<CertNetFetcher::Request> net_ocsp_request =
          net_fetcher->FetchOcsp(get_url, CertNetFetcher::DEFAULT,
                                 CertNetFetcher::DEFAULT);

      Error net_error;
      std::vector<uint8_t> ocsp_response_bytes;
      net_ocsp_request->WaitForResult(&net_error, &ocsp_response_bytes);

      if (net_error != OK)
        continue;

      bssl::OCSPVerifyResult::ResponseStatus response_details;

      bssl::OCSPRevocationStatus ocsp_status = bssl::CheckOCSP(
          std::string_view(
              reinterpret_cast<const char*>(ocsp_response_bytes.data()),
              ocsp_response_bytes.size()),
          cert, issuer_cert, time_now, max_age_seconds, &response_details);

      switch (ocsp_status) {
        case bssl::OCSPRevocationStatus::REVOKED:
          MarkCertificateRevoked(cert_errors);
          return false;
        case bssl::OCSPRevocationStatus::GOOD:
          return true;
        case bssl::OCSPRevocationStatus::UNKNOWN:
          break;
      }
    }
  }

  // Check CRLs.
  bssl::ParsedExtension crl_dp_extension;
  if (policy.crl_allowed &&
      cert->GetExtension(bssl::der::Input(bssl::kCrlDistributionPointsOid),
                         &crl_dp_extension)) {
    std::vector<bssl::ParsedDistributionPoint> distribution_points;
    if (ParseCrlDistributionPoints(crl_dp_extension.value,
                                   &distribution_points)) {
      for (const auto& distribution_point : distribution_points) {
        if (distribution_point.crl_issuer) {
          // Ignore indirect CRLs (CRL where CRLissuer != cert issuer), which
          // are optional according to RFC 5280's profile.
          continue;
        }

        if (distribution_point.reasons) {
          // Ignore CRLs that only contain some reasons. RFC 5280's profile
          // requires that conforming CAs "MUST include at least one
          // DistributionPoint that points to a CRL that covers the certificate
          // for all reasons".
          continue;
        }

        if (!distribution_point.distribution_point_fullname) {
          // Only distributionPoints with a fullName containing URIs are
          // supported.
          continue;
        }

        for (const auto& crl_uri :
             distribution_point.distribution_point_fullname
                 ->uniform_resource_identifiers) {
          // Only consider http:// URLs (https:// could create a circular
          // dependency).
          GURL parsed_crl_url(crl_uri);
          if (!parsed_crl_url.is_valid() ||
              !parsed_crl_url.SchemeIs(url::kHttpScheme)) {
            continue;
          }

          found_revocation_info = true;

          // Check the deadline after setting found_revocation_info, to not give
          // a misleading kNoRevocationMechanism failure.
          if (!deadline.is_null() && base::TimeTicks::Now() > deadline)
            break;

          if (!policy.networking_allowed)
            continue;

          if (!net_fetcher) {
            LOG(ERROR) << "Cannot fetch CRL as didn't specify a |net_fetcher|";
            continue;
          }

          // Fetch it over network.
          //
          // Note that no attempt is made to refetch without cache if a cached
          // CRL is too old, nor is there a separate CRL cache. It is assumed
          // the CRL server will send reasonable HTTP caching headers.
          std::unique_ptr<CertNetFetcher::Request> net_crl_request =
              net_fetcher->FetchCrl(parsed_crl_url, CertNetFetcher::DEFAULT,
                                    CertNetFetcher::DEFAULT);

          Error net_error;
          std::vector<uint8_t> crl_response_bytes;
          net_crl_request->WaitForResult(&net_error, &crl_response_bytes);

          if (net_error != OK)
            continue;

          bssl::CRLRevocationStatus crl_status = CheckCRL(
              std::string_view(
                  reinterpret_cast<const char*>(crl_response_bytes.data()),
                  crl_response_bytes.size()),
              certs, target_cert_index, distribution_point, time_now,
              max_age_seconds);

          switch (crl_status) {
            case bssl::CRLRevocationStatus::REVOKED:
              MarkCertificateRevoked(cert_errors);
              return false;
            case bssl::CRLRevocationStatus::GOOD:
              return true;
            case bssl::CRLRevocationStatus::UNKNOWN:
              break;
          }
        }
      }
    }
  }

  // Reaching here means that revocation checking was inconclusive. Determine
  // whether failure to complete revocation checking constitutes an error.

  if (!found_revocation_info) {
    if (policy.allow_missing_info) {
      // If the certificate lacked any (recognized) revocation mechanisms, and
      // the policy permits it, consider revocation checking a success.
      return true;
    } else {
      // If the certificate lacked any (recognized) revocation mechanisms, and
      // the policy forbids it, fail revocation checking.
      cert_errors->AddError(bssl::cert_errors::kNoRevocationMechanism);
      return false;
    }
  }

  // In soft-fail mode permit other failures.
  // TODO(eroman): Add a warning to |cert_errors| indicating the failure.
  if (policy.allow_unable_to_check)
    return true;

  // Otherwise the policy doesn't allow revocation checking to fail.
  cert_errors->AddError(bssl::cert_errors::kUnableToCheckRevocation);
  return false;
}

}  // namespace

void CheckValidatedChainRevocation(
    const bssl::ParsedCertificateList& certs,
    const RevocationPolicy& policy,
    base::TimeTicks deadline,
    std::string_view stapled_leaf_ocsp_response,
    base::Time current_time,
    CertNetFetcher* net_fetcher,
    bssl::CertPathErrors* errors,
    bssl::OCSPVerifyResult* stapled_ocsp_verify_result) {
  if (stapled_ocsp_verify_result)
    *stapled_ocsp_verify_result = bssl::OCSPVerifyResult();

  // Check each certificate for revocation using OCSP/CRL. Checks proceed
  // from the root certificate towards the leaf certificate. Revocation errors
  // are added to |errors|.
  for (size_t reverse_i = 0; reverse_i < certs.size(); ++reverse_i) {
    size_t i = certs.size() - reverse_i - 1;

    // Trust anchors bypass OCSP/CRL revocation checks. (The only way to revoke
    // trust anchors is via CRLSet or the built-in SPKI block list). Since
    // |certs| must be a validated chain, the final cert must be a trust
    // anchor.
    if (reverse_i == 0)
      continue;

    // TODO(eroman): Plumb stapled OCSP for non-leaf certificates from TLS?
    std::string_view stapled_ocsp =
        (i == 0) ? stapled_leaf_ocsp_response : std::string_view();

    std::optional<int64_t> max_age_seconds;
    if (policy.enforce_baseline_requirements) {
      max_age_seconds = ((i == 0) ? kMaxRevocationLeafUpdateAge
                                  : kMaxRevocationIntermediateUpdateAge)
                            .InSeconds();
    }

    // Check whether this certificate's revocation status complies with the
    // policy.
    bool cert_ok = CheckCertRevocation(
        certs, i, policy, deadline, stapled_ocsp, max_age_seconds, current_time,
        net_fetcher, errors->GetErrorsForCert(i),
        (i == 0) ? stapled_ocsp_verify_result : nullptr);

    if (!cert_ok) {
      // If any certificate in the chain fails revocation checks, the chain is
      // revoked and no need to check revocation status for the remaining
      // certificates.
      DCHECK(errors->GetErrorsForCert(i)->ContainsAnyErrorWithSeverity(
          bssl::CertError::SEVERITY_HIGH));
      break;
    }
  }
}

CRLSet::Result CheckChainRevocationUsingCRLSet(
    const CRLSet* crl_set,
    const bssl::ParsedCertificateList& certs,
    bssl::CertPathErrors* errors) {
  // Iterate from the root certificate towards the leaf (the root certificate is
  // also checked for revocation by CRLSet).
  std::string issuer_spki_hash;
  for (size_t reverse_i = 0; reverse_i < certs.size(); ++reverse_i) {
    size_t i = certs.size() - reverse_i - 1;
    const bssl::ParsedCertificate* cert = certs[i].get();

    // True if |cert| is the root of the chain.
    const bool is_root = reverse_i == 0;
    // True if |cert| is the leaf certificate of the chain.
    const bool is_target = i == 0;

    // Check for revocation using the certificate's SPKI.
    std::string spki_hash =
        crypto::SHA256HashString(cert->tbs().spki_tlv.AsStringView());
    CRLSet::Result result = crl_set->CheckSPKI(spki_hash);

    // Check for revocation using the certificate's Subject.
    if (result != CRLSet::REVOKED) {
      result = crl_set->CheckSubject(cert->tbs().subject_tlv.AsStringView(),
                                     spki_hash);
    }

    // Check for revocation using the certificate's serial number and issuer's
    // SPKI.
    if (result != CRLSet::REVOKED && !is_root) {
      result = crl_set->CheckSerial(cert->tbs().serial_number.AsStringView(),
                                    issuer_spki_hash);
    }

    // Prepare for the next iteration.
    issuer_spki_hash = std::move(spki_hash);

    switch (result) {
      case CRLSet::REVOKED:
        MarkCertificateRevoked(errors->GetErrorsForCert(i));
        return CRLSet::Result::REVOKED;
      case CRLSet::UNKNOWN:
        // If the status is unknown, advance to the subordinate certificate.
        break;
      case CRLSet::GOOD:
        if (is_target && !crl_set->IsExpired()) {
          // If the target is covered by the CRLSet and known good, consider
          // the entire chain to be valid (even though the revocation status
          // of the intermediates may have been UNKNOWN).
          //
          // Only the leaf certificate is considered for coverage because some
          // intermediates have CRLs with no revocations (after filtering) and
          // those CRLs are pruned from the CRLSet at generation time.
          return CRLSet::Result::GOOD;
        }
        break;
    }
  }

  // If no certificate was revoked, and the target was not known good, then
  // the revocation status is still unknown.
  return CRLSet::Result::UNKNOWN;
}

}  // namespace net
