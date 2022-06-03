// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_android.h"

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"
#include "net/android/cert_verify_result_android.h"
#include "net/android/network_library.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/known_roots.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
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

// Starting at certs[start], this function searches |certs| for an issuer of
// certs[start], then for an issuer of that issuer, and so on until it finds a
// certificate |cert| for which |certs| does not contain an issuer of
// |cert|. Returns a pointer to this |cert|, or nullptr if all certificates
// while path-building from |start| have an issuer in |certs| (including if
// there is a loop). Note that the returned certificate will be equal to |start|
// if |start| does not have an issuer in |certs|.
//
// TODO(estark): when searching for an issuer, this always uses the first
// encountered issuer in |certs|, and does not handle the situation where
// |certs| contains more than one issuer for a given certificate.
scoped_refptr<ParsedCertificate> FindLastCertWithUnknownIssuer(
    const ParsedCertificateList& certs,
    const scoped_refptr<ParsedCertificate>& start) {
  DCHECK_GE(certs.size(), 1u);
  std::set<scoped_refptr<ParsedCertificate>> used_in_path;
  scoped_refptr<ParsedCertificate> last = start;
  while (true) {
    used_in_path.insert(last);
    scoped_refptr<ParsedCertificate> last_issuer;
    // Find an issuer for |last| (which might be |last| itself if self-signed).
    for (const auto& cert : certs) {
      if (cert->normalized_subject() == last->normalized_issuer()) {
        last_issuer = cert;
        break;
      }
    }
    if (!last_issuer) {
      // There is no issuer for |last| in |certs|.
      return last;
    }
    if (last_issuer->normalized_subject() == last_issuer->normalized_issuer()) {
      // A chain can be built from |start| to a self-signed certificate, so
      // return nullptr to indicate that there is no certificate with an unknown
      // issuer.
      return nullptr;
    }
    if (used_in_path.find(last_issuer) != used_in_path.end()) {
      // |certs| contains a loop.
      return nullptr;
    }
    // Continue the search for |last_issuer|'s issuer.
    last = last_issuer;
  }
  NOTREACHED();
  return nullptr;
}

// Uses |fetcher| to fetch issuers from |uri|. If the fetch succeeds, the
// certificate is parsed and added to |cert_list|. Returns true if the fetch was
// successful and the result could be parsed as a certificate, and false
// otherwise.
bool PerformAIAFetchAndAddResultToVector(scoped_refptr<CertNetFetcher> fetcher,
                                         base::StringPiece uri,
                                         ParsedCertificateList* cert_list) {
  GURL url(uri);
  if (!url.is_valid())
    return false;
  std::unique_ptr<CertNetFetcher::Request> request(fetcher->FetchCaIssuers(
      url, CertNetFetcher::DEFAULT, CertNetFetcher::DEFAULT));
  Error error;
  std::vector<uint8_t> aia_fetch_bytes;
  request->WaitForResult(&error, &aia_fetch_bytes);
  base::UmaHistogramSparse("Net.Certificate.AndroidAIAFetchError",
                           std::abs(error));
  if (error != OK)
    return false;
  CertErrors errors;
  return ParsedCertificate::CreateAndAddToVector(
      x509_util::CreateCryptoBuffer(aia_fetch_bytes.data(),
                                    aia_fetch_bytes.size()),
      x509_util::DefaultParseCertificateOptions(), cert_list, &errors);
}

// Uses android::VerifyX509CertChain() to verify the certificates in |certs| for
// |hostname| and returns the verification status. If the verification was
// successful, this function populates |verify_result| and |verified_chain|;
// otherwise it leaves them untouched.
android::CertVerifyStatusAndroid AttemptVerificationAfterAIAFetch(
    const ParsedCertificateList& certs,
    const std::string& hostname,
    CertVerifyResult* verify_result,
    std::vector<std::string>* verified_chain) {
  std::vector<std::string> cert_bytes;
  for (const auto& cert : certs) {
    cert_bytes.push_back(cert->der_cert().AsString());
  }

  bool is_issued_by_known_root;
  std::vector<std::string> candidate_verified_chain;
  android::CertVerifyStatusAndroid status;
  android::VerifyX509CertChain(cert_bytes, kAuthType, hostname, &status,
                               &is_issued_by_known_root,
                               &candidate_verified_chain);

  if (status == android::CERT_VERIFY_STATUS_ANDROID_OK) {
    verify_result->is_issued_by_known_root = is_issued_by_known_root;
    *verified_chain = candidate_verified_chain;
  }
  return status;
}

// After a CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT error is encountered, this
// function can be called to fetch intermediates and retry verification.
//
// It will start from the first certificate in |cert_bytes| and construct a
// chain as far as it can using certificates in |cert_bytes|, and then
// iteratively fetch issuers from any AIA URLs in the last certificate in this
// chain. It will fetch issuers until it encounters a chain that verifies with
// status CERT_VERIFY_STATUS_ANDROID_OK, or it runs out of AIA URLs to fetch, or
// it has attempted |kMaxAIAFetches| fetches.
//
// If it finds a chain that verifies successfully, it returns
// CERT_VERIFY_STATUS_ANDROID_OK and sets |verify_result| and |verified_chain|
// correspondingly. Otherwise, it returns
// CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT and does not modify
// |verify_result| or |verified_chain|.
android::CertVerifyStatusAndroid TryVerifyWithAIAFetching(
    const std::vector<std::string>& cert_bytes,
    const std::string& hostname,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    CertVerifyResult* verify_result,
    std::vector<std::string>* verified_chain) {
  if (!cert_net_fetcher)
    return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;

  // Convert the certificates into ParsedCertificates for ease of pulling out
  // AIA URLs.
  CertErrors errors;
  ParsedCertificateList certs;
  for (const auto& cert : cert_bytes) {
    if (!ParsedCertificate::CreateAndAddToVector(
            x509_util::CreateCryptoBuffer(cert),
            x509_util::DefaultParseCertificateOptions(), &certs, &errors)) {
      return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
    }
  }

  // Build a chain as far as possible from the target certificate at index 0,
  // using the initially provided certificates.
  scoped_refptr<ParsedCertificate> last_cert_with_unknown_issuer =
      FindLastCertWithUnknownIssuer(certs, certs[0].get());
  if (!last_cert_with_unknown_issuer) {
    // |certs| either contains a loop, or contains a full chain to a self-signed
    // certificate. Do not attempt AIA fetches for such a chain.
    return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
  }

  unsigned int num_aia_fetches = 0;
  while (true) {
    // If chain-building has terminated in a certificate that does not have an
    // AIA URL, give up.
    //
    // TODO(estark): Instead of giving up at this point, it would be more robust
    // to go back to the certificate before |last_cert| in the chain and attempt
    // an AIA fetch from that point (if one hasn't already been done). This
    // would accomodate chains where the server serves Leaf -> I1 signed by a
    // root not in the client's trust store, but AIA fetching would yield an
    // intermediate I2 signed by a root that *is* in the client's trust store.
    if (!last_cert_with_unknown_issuer->has_authority_info_access())
      return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;

    for (const auto& uri : last_cert_with_unknown_issuer->ca_issuers_uris()) {
      num_aia_fetches++;
      if (num_aia_fetches > kMaxAIAFetches)
        return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
      if (!PerformAIAFetchAndAddResultToVector(cert_net_fetcher, uri, &certs))
        continue;
      android::CertVerifyStatusAndroid status =
          AttemptVerificationAfterAIAFetch(certs, hostname, verify_result,
                                           verified_chain);
      if (status == android::CERT_VERIFY_STATUS_ANDROID_OK)
        return status;
    }

    // If verification still failed but the path expanded, continue to attempt
    // AIA fetches.
    scoped_refptr<ParsedCertificate> new_last_cert_with_unknown_issuer =
        FindLastCertWithUnknownIssuer(certs, last_cert_with_unknown_issuer);
    if (!new_last_cert_with_unknown_issuer ||
        new_last_cert_with_unknown_issuer == last_cert_with_unknown_issuer) {
      // The last round of AIA fetches (if there were any) didn't expand the
      // path, or it did such that |certs| now contains a full path to an
      // (untrusted) root or a loop.
      //
      // TODO(estark): As above, it would be more robust to go back one
      // certificate and attempt an AIA fetch from that point.
      return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
    }
    last_cert_with_unknown_issuer = new_last_cert_with_unknown_issuer;
  }

  NOTREACHED();
  return android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT;
}

// Returns true if the certificate verification call was successful (regardless
// of its result), i.e. if |verify_result| was set. Otherwise returns false.
bool VerifyFromAndroidTrustManager(
    const std::vector<std::string>& cert_bytes,
    const std::string& hostname,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    CertVerifyResult* verify_result) {
  android::CertVerifyStatusAndroid status;
  std::vector<std::string> verified_chain;

  android::VerifyX509CertChain(cert_bytes, kAuthType, hostname, &status,
                               &verify_result->is_issued_by_known_root,
                               &verified_chain);

  // If verification resulted in a NO_TRUSTED_ROOT error, then fetch
  // intermediates and retry.
  if (status == android::CERT_VERIFY_STATUS_ANDROID_NO_TRUSTED_ROOT) {
    status = TryVerifyWithAIAFetching(cert_bytes, hostname,
                                      std::move(cert_net_fetcher),
                                      verify_result, &verified_chain);
    UMA_HISTOGRAM_BOOLEAN(
        "Net.Certificate.VerificationSuccessAfterAIAFetchingNeeded",
        status == android::CERT_VERIFY_STATUS_ANDROID_OK);
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
      verify_result->cert_status |= CERT_STATUS_INVALID;
      break;
  }

  // Save the verified chain.
  if (!verified_chain.empty()) {
    std::vector<base::StringPiece> verified_chain_pieces(verified_chain.size());
    for (size_t i = 0; i < verified_chain.size(); i++) {
      verified_chain_pieces[i] = base::StringPiece(verified_chain[i]);
    }
    scoped_refptr<X509Certificate> verified_cert =
        X509Certificate::CreateFromDERCertChain(verified_chain_pieces);
    if (verified_cert.get())
      verify_result->verified_cert = std::move(verified_cert);
    else
      verify_result->cert_status |= CERT_STATUS_INVALID;
  }

  // Extract the public key hashes and check whether or not any are known
  // roots. Walk from the end of the chain (root) to leaf, to optimize for
  // known root checks.
  for (auto it = verified_chain.rbegin(); it != verified_chain.rend(); ++it) {
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(*it, &spki_bytes)) {
      verify_result->cert_status |= CERT_STATUS_INVALID;
      continue;
    }

    HashValue sha256(HASH_VALUE_SHA256);
    crypto::SHA256HashString(spki_bytes, sha256.data(), crypto::kSHA256Length);
    verify_result->public_key_hashes.push_back(sha256);

    if (!verify_result->is_issued_by_known_root) {
      verify_result->is_issued_by_known_root =
          GetNetTrustAnchorHistogramIdForSPKI(sha256) != 0;
    }
  }

  // Reverse the hash list, to maintain the leaf->root ordering.
  std::reverse(verify_result->public_key_hashes.begin(),
               verify_result->public_key_hashes.end());

  return true;
}

void GetChainDEREncodedBytes(X509Certificate* cert,
                             std::vector<std::string>* chain_bytes) {
  chain_bytes->reserve(1 + cert->intermediate_buffers().size());
  chain_bytes->emplace_back(
      net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  for (const auto& handle : cert->intermediate_buffers()) {
    chain_bytes->emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(handle.get()));
  }
}

}  // namespace

CertVerifyProcAndroid::CertVerifyProcAndroid(
    scoped_refptr<CertNetFetcher> cert_net_fetcher)
    : cert_net_fetcher_(std::move(cert_net_fetcher)) {}

CertVerifyProcAndroid::~CertVerifyProcAndroid() {}

bool CertVerifyProcAndroid::SupportsAdditionalTrustAnchors() const {
  return false;
}

int CertVerifyProcAndroid::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  std::vector<std::string> cert_bytes;
  GetChainDEREncodedBytes(cert, &cert_bytes);
  if (!VerifyFromAndroidTrustManager(cert_bytes, hostname, cert_net_fetcher_,
                                     verify_result)) {
    NOTREACHED();
    return ERR_FAILED;
  }

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  LogNameNormalizationMetrics(".Android", verify_result->verified_cert.get(),
                              verify_result->is_issued_by_known_root);

  return OK;
}

}  // namespace net
