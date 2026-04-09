// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_android.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_view_util.h"
#include "crypto/hash.h"
#include "net/android/cert_verify_result_android.h"
#include "net/android/network_library.h"
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/known_roots.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "url/gurl.h"

namespace net {

namespace {

// Android ignores the authType parameter to
// X509TrustManager.checkServerTrusted, so pass in a dummy value. See
// https://crbug.com/627154.
const char kAuthType[] = "RSA";

// The maximum number of AIA fetches that TryVerifyWithAIAFetching() will
// attempt. If a valid chain cannot be built after this many fetches,
// TryVerifyWithAIAFetching() will give up and return
// CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT.
const unsigned int kMaxAIAFetches = 5;

// Uses |fetcher| to fetch issuers from |uri|. If the fetch succeeds, the
// certificate is parsed and added to |cert_list|. Returns true if the fetch was
// successful and the result could be parsed as a certificate, and false
// otherwise.
bool PerformAIAFetchAndAddResultToVector(
    scoped_refptr<CertNetFetcher> fetcher,
    std::string_view uri,
    bssl::ParsedCertificateList* cert_list) {
  GURL url(uri);
  if (!url.is_valid())
    return false;
  std::unique_ptr<CertNetFetcher::Request> request(fetcher->FetchCaIssuers(
      url, CertNetFetcher::DEFAULT, CertNetFetcher::DEFAULT));
  Error error;
  std::vector<uint8_t> aia_fetch_bytes;
  request->WaitForResult(&error, &aia_fetch_bytes);
  if (error != OK)
    return false;
  bssl::CertErrors errors;
  return bssl::ParsedCertificate::CreateAndAddToVector(
      x509_util::CreateCryptoBuffer(aia_fetch_bytes),
      x509_util::DefaultParseCertificateOptions(), cert_list, &errors);
}

// Uses android::VerifyX509CertChain() to verify the certificates in |certs| for
// |hostname| and returns the verification status. If the verification was
// successful, this function populates |verify_result| and |verified_chain|;
// otherwise it leaves them untouched.
android::CertVerifyStatusAndroid AttemptVerificationAfterAIAFetch(
    const bssl::ParsedCertificateList& certs,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    CertVerifyResult* verify_result,
    std::vector<std::string>* verified_chain) {
  std::vector<std::string> cert_bytes;
  for (const auto& cert : certs) {
    cert_bytes.emplace_back(base::as_string_view(cert->der_cert()));
  }

  bool is_issued_by_known_root;
  std::vector<std::string> candidate_verified_chain;
  android::CertVerifyStatusAndroid status;
  android::VerifyX509CertChain(cert_bytes, kAuthType, hostname, ocsp_response,
                               sct_list, &status, &is_issued_by_known_root,
                               &candidate_verified_chain);

  if (status == android::CERT_VERIFY_STATUS_ANDROID_OK) {
    verify_result->is_issued_by_known_root = is_issued_by_known_root;
    *verified_chain = candidate_verified_chain;
  }
  return status;
}

// Searches |certs| for a certificate whose subject matches |cert|'s issuer.
// Returns the matching certificate if found, nullptr otherwise.
//
// TODO(estark): when searching for an issuer, this always returns the first
// encountered match in |certs|, and does not handle the situation where
// |certs| contains more than one issuer for a given certificate.
std::shared_ptr<const bssl::ParsedCertificate> FindIssuerInCerts(
    const std::shared_ptr<const bssl::ParsedCertificate>& cert,
    const bssl::ParsedCertificateList& certs) {
  for (const auto& candidate : certs) {
    if (candidate != cert &&
        candidate->normalized_subject() == cert->normalized_issuer()) {
      return candidate;
    }
  }
  return nullptr;
}

// Returns true if |cert| has any AIA ca_issuers URLs that are not in
// |fetched_aia_urls|.
bool HasUntriedAIAURL(
    const std::shared_ptr<const bssl::ParsedCertificate>& cert,
    const absl::flat_hash_set<std::string>& fetched_aia_urls) {
  if (!cert->has_authority_info_access() || cert->ca_issuers_uris().empty()) {
    return false;
  }
  for (const auto& uri : cert->ca_issuers_uris()) {
    if (!fetched_aia_urls.contains(std::string(uri))) {
      return true;
    }
  }
  return false;
}

// Walks the certificate chain starting from |certs[0]| and returns the next
// certificate whose AIA URL should be fetched. Prioritizes the deepest
// certificate with an unknown issuer. If that certificate has no untried AIA
// URLs, backtracks to earlier certificates with untried AIA URLs.
//
// |certs|: The current certificate collection.
// |fetched_aia_urls|: URLs that have already been fetched.
//
// Returns the next certificate to try fetching AIA from, or nullptr if none.
std::shared_ptr<const bssl::ParsedCertificate> GetNextAIACandidate(
    const bssl::ParsedCertificateList& certs,
    const absl::flat_hash_set<std::string>& fetched_aia_urls) {
  std::vector<std::shared_ptr<const bssl::ParsedCertificate>>
      earlier_candidates;
  std::shared_ptr<const bssl::ParsedCertificate> current = certs[0];
  absl::flat_hash_set<std::shared_ptr<const bssl::ParsedCertificate>> visited;

  // The visited check detects loops in the certificate chain.
  while (current && !visited.contains(current)) {
    visited.insert(current);

    // Remember earlier certs with untried AIA for potential backtracking.
    if (HasUntriedAIAURL(current, fetched_aia_urls)) {
      earlier_candidates.push_back(current);
    }

    auto issuer = FindIssuerInCerts(current, certs);
    if (!issuer) {
      // No issuer found - this is the deepest cert with unknown issuer.
      // If it has untried AIA, use it. Otherwise, backtrack to earlier certs.
      if (HasUntriedAIAURL(current, fetched_aia_urls)) {
        return current;
      }
      // Return the deepest earlier cert with untried AIA.
      if (!earlier_candidates.empty()) {
        return earlier_candidates.back();
      }
      return nullptr;
    }

    if (issuer->normalized_subject() == issuer->normalized_issuer()) {
      // Self-signed cert - chain terminates here. Backtrack to earlier certs
      // with untried AIA URLs if available.
      if (!earlier_candidates.empty()) {
        return earlier_candidates.back();
      }
      return nullptr;
    }

    current = issuer;
  }

  // Loop detected in the certificate chain. Try backtracking to an earlier
  // cert with untried AIA.
  if (!earlier_candidates.empty()) {
    return earlier_candidates.back();
  }
  return nullptr;
}

// Checks if a certificate with the same DER encoding already exists in |certs|.
bool CertExistsInCollection(
    const std::shared_ptr<const bssl::ParsedCertificate>& cert,
    const bssl::ParsedCertificateList& certs) {
  for (const auto& existing : certs) {
    if (existing->der_cert() == cert->der_cert()) {
      return true;
    }
  }
  return false;
}

// After a CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT error is encountered, this
// function can be called to fetch intermediates and retry verification.
//
// It will start from the first certificate in |cert_bytes| and construct a
// chain as far as it can using certificates in |cert_bytes|, and then
// iteratively fetch issuers from AIA URLs. When the deepest certificate has no
// AIA or its AIA fails, it backtracks to try AIA URLs from earlier certificates
// in the chain. It will fetch issuers until it encounters a chain that verifies
// with status CERT_VERIFY_STATUS_ANDROID_OK, or it runs out of AIA URLs to
// fetch, or it has attempted |kMaxAIAFetches| fetches.
//
// If it finds a chain that verifies successfully, it returns
// CERT_VERIFY_STATUS_ANDROID_OK and sets |verify_result| and |verified_chain|
// correspondingly. Otherwise, it returns
// CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT and does not modify
// |verify_result| or |verified_chain|.
android::CertVerifyStatusAndroid TryVerifyWithAIAFetching(
    const std::vector<std::string>& cert_bytes,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    CertVerifyResult* verify_result,
    std::vector<std::string>* verified_chain) {
  if (!cert_net_fetcher) {
    return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
  }

  // Convert the certificates into ParsedCertificates for ease of pulling out
  // AIA URLs.
  bssl::CertErrors errors;
  bssl::ParsedCertificateList certs;
  for (const auto& cert : cert_bytes) {
    if (!bssl::ParsedCertificate::CreateAndAddToVector(
            x509_util::CreateCryptoBuffer(cert),
            x509_util::DefaultParseCertificateOptions(), &certs, &errors)) {
      return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
    }
  }

  if (certs.empty()) {
    return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
  }

  // Track which AIA URLs we've already fetched to avoid redundant fetches.
  absl::flat_hash_set<std::string> fetched_aia_urls;
  unsigned int num_aia_fetches = 0;

  while (num_aia_fetches < kMaxAIAFetches) {
    auto cert_to_try = GetNextAIACandidate(certs, fetched_aia_urls);

    if (!cert_to_try) {
      break;
    }

    for (const auto& uri : cert_to_try->ca_issuers_uris()) {
      std::string uri_string(uri);

      // Skip URLs we've already fetched.
      if (fetched_aia_urls.contains(uri_string)) {
        continue;
      }

      fetched_aia_urls.insert(uri_string);
      num_aia_fetches++;

      if (num_aia_fetches > kMaxAIAFetches) {
        return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
      }

      // Fetch the certificate from the AIA URL.
      bssl::ParsedCertificateList fetched_certs;
      if (!PerformAIAFetchAndAddResultToVector(cert_net_fetcher, uri,
                                               &fetched_certs)) {
        continue;
      }

      // Add any fetched certs that aren't duplicates. An AIA response may
      // contain more than one certificate. If none were actually added,
      // skip re-verification since the cert list hasn't changed.
      bool any_added = false;
      for (const auto& fetched_cert : fetched_certs) {
        if (!CertExistsInCollection(fetched_cert, certs)) {
          certs.push_back(fetched_cert);
          any_added = true;
        }
      }
      if (!any_added) {
        continue;
      }

      android::CertVerifyStatusAndroid status =
          AttemptVerificationAfterAIAFetch(certs, hostname, ocsp_response,
                                           sct_list, verify_result,
                                           verified_chain);

      if (status == android::CERT_VERIFY_STATUS_ANDROID_OK) {
        return status;
      }

      // Continue trying - the newly fetched cert might enable new AIA paths
    }
  }

  return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
}

// Returns true if the certificate verification call was successful (regardless
// of its result), i.e. if |verify_result| was set. Otherwise returns false.
bool VerifyFromAndroidTrustManager(
    const std::vector<std::string>& cert_bytes,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    CertVerifyResult* verify_result) {
  android::CertVerifyStatusAndroid status =
      android::CERT_VERIFY_STATUS_ANDROID_FAILED;
  std::vector<std::string> verified_chain;

  android::VerifyX509CertChain(
      cert_bytes, kAuthType, hostname, ocsp_response, sct_list, &status,
      &verify_result->is_issued_by_known_root, &verified_chain);

  // If verification resulted in a NO_TRUSTED_ROOT error, then fetch
  // intermediates and retry.
  if (status == android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT &&
      !(flags & CertVerifyProc::VERIFY_DISABLE_NETWORK_FETCHES)) {
    status = TryVerifyWithAIAFetching(cert_bytes, hostname, ocsp_response,
                                      sct_list, std::move(cert_net_fetcher),
                                      verify_result, &verified_chain);
  }

  switch (status) {
    case android::CERT_VERIFY_STATUS_ANDROID_FAILED:
      return false;
    case android::CERT_VERIFY_STATUS_ANDROID_OK:
      break;
    case android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT:
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;
    case android::CERT_VERIFY_STATUS_ANDROID_EXPIRED:
    case android::CERT_VERIFY_STATUS_ANDROID_NOT_YET_VALID:
      verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
      break;
    case android::CERT_VERIFY_STATUS_ANDROID_UNABLE_TO_PARSE:
      verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
    case android::CERT_VERIFY_STATUS_ANDROID_INCORRECT_KEY_USAGE:
      verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
    default:
      NOTREACHED();
  }

  // Save the verified chain.
  if (!verified_chain.empty()) {
    std::vector<std::string_view> verified_chain_pieces(verified_chain.size());
    for (size_t i = 0; i < verified_chain.size(); i++) {
      verified_chain_pieces[i] = std::string_view(verified_chain[i]);
    }
    scoped_refptr<X509Certificate> verified_cert =
        X509Certificate::CreateFromDERCertChain(verified_chain_pieces);
    if (verified_cert.get())
      verify_result->verified_cert = std::move(verified_cert);
    else
      verify_result->cert_status |= CERT_STATUS_INVALID;
  } else if (!IsCertStatusError(verify_result->cert_status)) {
    // If the verified chain is empty and the return status was OK, that's
    // actually an error. But don't add the cert_status flag if the chain is
    // empty and it already was marked as an error, since that would hide the
    // actual error reason.
    verify_result->cert_status |= CERT_STATUS_INVALID;
  }

  // Extract the public key hashes and check whether or not any are known
  // roots. Walk from the end of the chain (root) to leaf, to optimize for
  // known root checks.
  for (const auto& cert : base::Reversed(verified_chain)) {
    std::string_view spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(cert, &spki_bytes)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      continue;
    }

    SHA256HashValue sha256(
        crypto::hash::Sha256(base::as_byte_span(spki_bytes)));
    verify_result->public_key_hashes.push_back(sha256);

    if (!verify_result->is_issued_by_known_root) {
      verify_result->is_issued_by_known_root =
          GetNetTrustAnchorHistogramIdForSPKI(sha256) != 0;
    }
  }

  // Reverse the hash list, to maintain the leaf->root ordering.
  std::ranges::reverse(verify_result->public_key_hashes);

  return true;
}

void GetChainDEREncodedBytes(X509Certificate* cert,
                             std::vector<std::string>* chain_bytes) {
  chain_bytes->reserve(cert->cert_buffers().size());
  for (const auto& handle : cert->cert_buffers()) {
    chain_bytes->emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(handle.get()));
  }
}

}  // namespace

CertVerifyProcAndroid::CertVerifyProcAndroid(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    scoped_refptr<CRLSet> crl_set)
    : CertVerifyProc(std::move(crl_set)),
      cert_net_fetcher_(std::move(cert_net_fetcher)) {}

CertVerifyProcAndroid::~CertVerifyProcAndroid() = default;

int CertVerifyProcAndroid::VerifyInternal(X509Certificate* cert,
                                          const std::string& hostname,
                                          const std::string& ocsp_response,
                                          const std::string& sct_list,
                                          int flags,
                                          CertVerifyResult* verify_result,
                                          const NetLogWithSource& net_log) {
  std::vector<std::string> cert_bytes;
  GetChainDEREncodedBytes(cert, &cert_bytes);
  if (!VerifyFromAndroidTrustManager(cert_bytes, hostname, ocsp_response,
                                     sct_list, flags, cert_net_fetcher_,
                                     verify_result)) {
    return ERR_FAILED;
  }

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (TestRootCerts::HasInstance() &&
      !verify_result->verified_cert->intermediate_buffers().empty() &&
      TestRootCerts::GetInstance()->IsKnownRoot(x509_util::CryptoBufferAsSpan(
          verify_result->verified_cert->intermediate_buffers().back().get()))) {
    verify_result->is_issued_by_known_root = true;
  }

  LogNameNormalizationMetrics(".Android", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net
