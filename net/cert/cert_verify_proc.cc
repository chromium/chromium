// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
#include "crypto/sha2.h"
#include "net/base/cronet_buildflags.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/internal/revocation_checker.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/known_roots.h"
#include "net/cert/symantec_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/encode_values.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/ocsp.h"
#include "third_party/boringssl/src/pki/ocsp_revocation_status.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/cert_verify_proc_builtin.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif  // CHROME_ROOT_STORE_SUPPORTED

#if BUILDFLAG(IS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#elif BUILDFLAG(IS_IOS)
#include "net/cert/cert_verify_proc_ios.h"
#endif

namespace net {

namespace {

// Constants used to build histogram names
const char kLeafCert[] = "Leaf";
const char kIntermediateCert[] = "Intermediate";
const char kRootCert[] = "Root";

// Histogram buckets for RSA key sizes, as well as unknown key types. RSA key
// sizes < 1024 bits should cause errors, while key sizes > 16K are not
// supported by BoringSSL.
const int kRsaKeySizes[] = {512,  768,  1024, 1536, 2048,
                            3072, 4096, 8192, 16384};
// Histogram buckets for ECDSA key sizes. The list was historically based upon
// FIPS 186-4 approved curves, but most are impossible. BoringSSL will only ever
// return P-224, P-256, P-384, or P-521, and the verifier will reject P-224.
const int kEcdsaKeySizes[] = {163, 192, 224, 233, 256, 283, 384, 409, 521, 571};

const char* CertTypeToString(X509Certificate::PublicKeyType cert_type) {
  switch (cert_type) {
    case X509Certificate::kPublicKeyTypeUnknown:
      return "Unknown";
    case X509Certificate::kPublicKeyTypeRSA:
      return "RSA";
    case X509Certificate::kPublicKeyTypeECDSA:
      return "ECDSA";
  }
  NOTREACHED();
}

void RecordPublicKeyHistogram(const char* chain_position,
                              bool baseline_keysize_applies,
                              size_t size_bits,
                              X509Certificate::PublicKeyType cert_type) {
  std::string histogram_name =
      base::StringPrintf("CertificateType2.%s.%s.%s",
                         baseline_keysize_applies ? "BR" : "NonBR",
                         chain_position,
                         CertTypeToString(cert_type));
  // Do not use UMA_HISTOGRAM_... macros here, as it caches the Histogram
  // instance and thus only works if |histogram_name| is constant.
  base::HistogramBase* counter = nullptr;

  // Histogram buckets are contingent upon the underlying algorithm being used.
  switch (cert_type) {
    case X509Certificate::kPublicKeyTypeECDSA:
      counter = base::CustomHistogram::FactoryGet(
          histogram_name,
          base::CustomHistogram::ArrayToCustomEnumRanges(kEcdsaKeySizes),
          base::HistogramBase::kUmaTargetedHistogramFlag);
      break;
    case X509Certificate::kPublicKeyTypeRSA:
    case X509Certificate::kPublicKeyTypeUnknown:
      counter = base::CustomHistogram::FactoryGet(
          histogram_name,
          base::CustomHistogram::ArrayToCustomEnumRanges(kRsaKeySizes),
          base::HistogramBase::kUmaTargetedHistogramFlag);
      break;
  }
  counter->Add(size_bits);
}

// Returns true if |type| is |kPublicKeyTypeRSA| and if |size_bits| is < 1024.
// Note that this means there may be false negatives: keys for other algorithms
// and which are weak will pass this test.
bool IsWeakKey(X509Certificate::PublicKeyType type, size_t size_bits) {
  switch (type) {
    case X509Certificate::kPublicKeyTypeRSA:
      return size_bits < 1024;
    default:
      return false;
  }
}

// Returns true if |cert| contains a known-weak key. Additionally, histograms
// the observed keys for future tightening of the definition of what
// constitutes a weak key.
bool ExaminePublicKeys(const scoped_refptr<X509Certificate>& cert,
                       bool should_histogram) {
  // The effective date of the CA/Browser Forum's Baseline Requirements -
  // 2012-07-01 00:00:00 UTC.
  const base::Time kBaselineEffectiveDate =
      base::Time::FromInternalValue(INT64_C(12985574400000000));
  // The effective date of the key size requirements from Appendix A, v1.1.5
  // 2014-01-01 00:00:00 UTC.
  const base::Time kBaselineKeysizeEffectiveDate =
      base::Time::FromInternalValue(INT64_C(13033008000000000));

  size_t size_bits = 0;
  X509Certificate::PublicKeyType type = X509Certificate::kPublicKeyTypeUnknown;
  bool weak_key = false;
  bool baseline_keysize_applies =
      cert->valid_start() >= kBaselineEffectiveDate &&
      cert->valid_expiry() >= kBaselineKeysizeEffectiveDate;

  X509Certificate::GetPublicKeyInfo(cert->cert_buffer(), &size_bits, &type);
  if (should_histogram) {
    RecordPublicKeyHistogram(kLeafCert, baseline_keysize_applies, size_bits,
                             type);
  }
  if (IsWeakKey(type, size_bits))
    weak_key = true;

  const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& intermediates =
      cert->intermediate_buffers();
  for (size_t i = 0; i < intermediates.size(); ++i) {
    X509Certificate::GetPublicKeyInfo(intermediates[i].get(), &size_bits,
                                      &type);
    if (should_histogram) {
      RecordPublicKeyHistogram(
          (i < intermediates.size() - 1) ? kIntermediateCert : kRootCert,
          baseline_keysize_applies,
          size_bits,
          type);
    }
    if (!weak_key && IsWeakKey(type, size_bits))
      weak_key = true;
  }

  return weak_key;
}

void BestEffortCheckOCSP(const std::string& raw_response,
                         const X509Certificate& certificate,
                         bssl::OCSPVerifyResult* verify_result) {
  if (raw_response.empty()) {
    *verify_result = bssl::OCSPVerifyResult();
    verify_result->response_status = bssl::OCSPVerifyResult::MISSING;
    return;
  }

  std::string_view cert_der =
      x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer());

  // Try to get the certificate that signed |certificate|. This will run into
  // problems if the CertVerifyProc implementation doesn't return the ordered
  // certificates. If that happens the OCSP verification may be incorrect.
  std::string_view issuer_der;
  if (certificate.intermediate_buffers().empty()) {
    if (X509Certificate::IsSelfSigned(certificate.cert_buffer())) {
      issuer_der = cert_der;
    } else {
      // A valid cert chain wasn't provided.
      *verify_result = bssl::OCSPVerifyResult();
      return;
    }
  } else {
    issuer_der = x509_util::CryptoBufferAsStringPiece(
        certificate.intermediate_buffers().front().get());
  }

  verify_result->revocation_status = bssl::CheckOCSP(
      raw_response, cert_der, issuer_der, base::Time::Now().ToTimeT(),
      kMaxRevocationLeafUpdateAge.InSeconds(), &verify_result->response_status);
}

// Records details about the most-specific trust anchor in |hashes|, which is
// expected to be ordered with the leaf cert first and the root cert last.
// "Most-specific" refers to the case that it is not uncommon to have multiple
// potential trust anchors present in a chain, depending on the client trust
// store. For example, '1999-Root' cross-signing '2005-Root' cross-signing
// '2012-Root' cross-signing '2017-Root', then followed by intermediate and
// leaf. For purposes of assessing impact of, say, removing 1999-Root, while
// including 2017-Root as a trust anchor, then the validation should be
// counted as 2017-Root, rather than 1999-Root.
//
// This also accounts for situations in which a new CA is introduced, and
// has been cross-signed by an existing CA. Assessing impact should use the
// most-specific trust anchor, when possible.
//
// This also histograms for divergence between the root store and
// |spki_hashes| - that is, situations in which the OS methods of detecting
// a known root flag a certificate as known, but its hash is not known as part
// of the built-in list.
void RecordTrustAnchorHistogram(const HashValueVector& spki_hashes,
                                bool is_issued_by_known_root) {
  int32_t id = 0;
  for (const auto& hash : spki_hashes) {
    id = GetNetTrustAnchorHistogramIdForSPKI(hash);
    if (id != 0)
      break;
  }
  base::UmaHistogramSparse("Net.Certificate.TrustAnchor.Verify", id);

  // Record when a known trust anchor is not found within the chain, but the
  // certificate is flagged as being from a known root (meaning a fallback to
  // OS-based methods of determination).
  if (id == 0) {
    UMA_HISTOGRAM_BOOLEAN("Net.Certificate.TrustAnchor.VerifyOutOfDate",
                          is_issued_by_known_root);
  }
}

// Inspects the signature algorithms in a single certificate |cert|.
//
//   * Sets |verify_result->has_sha1| to true if the certificate uses SHA1.
//
// Returns false if the signature algorithm was unknown or mismatched.
[[nodiscard]] bool InspectSignatureAlgorithmForCert(
    const CRYPTO_BUFFER* cert,
    CertVerifyResult* verify_result) {
  std::string_view cert_algorithm_sequence;
  std::string_view tbs_algorithm_sequence;

  // Extract the AlgorithmIdentifier SEQUENCEs
  if (!asn1::ExtractSignatureAlgorithmsFromDERCert(
          x509_util::CryptoBufferAsStringPiece(cert), &cert_algorithm_sequence,
          &tbs_algorithm_sequence)) {
    return false;
  }

  std::optional<bssl::SignatureAlgorithm> cert_algorithm =
      bssl::ParseSignatureAlgorithm(bssl::der::Input(cert_algorithm_sequence));
  std::optional<bssl::SignatureAlgorithm> tbs_algorithm =
      bssl::ParseSignatureAlgorithm(bssl::der::Input(tbs_algorithm_sequence));
  if (!cert_algorithm || !tbs_algorithm || *cert_algorithm != *tbs_algorithm) {
    return false;
  }

  switch (*cert_algorithm) {
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha1:
    case bssl::SignatureAlgorithm::kEcdsaSha1:
      verify_result->has_sha1 = true;
      return true;  // For now.

    case bssl::SignatureAlgorithm::kRsaPkcs1Sha256:
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha384:
    case bssl::SignatureAlgorithm::kRsaPkcs1Sha512:
    case bssl::SignatureAlgorithm::kEcdsaSha256:
    case bssl::SignatureAlgorithm::kEcdsaSha384:
    case bssl::SignatureAlgorithm::kEcdsaSha512:
    case bssl::SignatureAlgorithm::kRsaPssSha256:
    case bssl::SignatureAlgorithm::kRsaPssSha384:
    case bssl::SignatureAlgorithm::kRsaPssSha512:
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// InspectSignatureAlgorithmsInChain() sets |verify_result->has_*| based on
// the signature algorithms used in the chain, and also checks that certificates
// don't have contradictory signature algorithms.
//
// Returns false if any signature algorithm in the chain is unknown or
// mismatched.
//
// Background:
//
// X.509 certificates contain two redundant descriptors for the signature
// algorithm; one is covered by the signature, but in order to verify the
// signature, the other signature algorithm is untrusted.
//
// RFC 5280 states that the two should be equal, in order to mitigate risk of
// signature substitution attacks, but also discourages verifiers from enforcing
// the profile of RFC 5280.
//
// System verifiers are inconsistent - some use the unsigned signature, some use
// the signed signature, and they generally do not enforce that both match. This
// creates confusion, as it's possible that the signature itself may be checked
// using algorithm A, but if subsequent consumers report the certificate
// algorithm, they may end up reporting algorithm B, which was not used to
// verify the certificate. This function enforces that the two signatures match
// in order to prevent such confusion.
[[nodiscard]] bool InspectSignatureAlgorithmsInChain(
    CertVerifyResult* verify_result) {
  const std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>& intermediates =
      verify_result->verified_cert->intermediate_buffers();

  // If there are no intermediates, then the leaf is trusted or verification
  // failed.
  if (intermediates.empty())
    return true;

  DCHECK(!verify_result->has_sha1);

  // Fill in hash algorithms for the leaf certificate.
  if (!InspectSignatureAlgorithmForCert(
          verify_result->verified_cert->cert_buffer(), verify_result)) {
    return false;
  }

  // Fill in hash algorithms for the intermediate cerificates, excluding the
  // final one (which is presumably the trust anchor; may be incorrect for
  // partial chains).
  for (size_t i = 0; i + 1 < intermediates.size(); ++i) {
    if (!InspectSignatureAlgorithmForCert(intermediates[i].get(),
                                          verify_result))
      return false;
  }

  return true;
}

base::Value::Dict CertVerifyParams(X509Certificate* cert,
                                   const std::string& hostname,
                                   const std::string& ocsp_response,
                                   const std::string& sct_list,
                                   int flags,
                                   CRLSet* crl_set) {
  base::Value::Dict dict;
  dict.Set("certificates", NetLogX509CertificateList(cert));
  if (!ocsp_response.empty()) {
    dict.Set("ocsp_response",
             bssl::PEMEncode(ocsp_response, "NETLOG OCSP RESPONSE"));
  }
  if (!sct_list.empty()) {
    dict.Set("sct_list", bssl::PEMEncode(sct_list, "NETLOG SCT LIST"));
  }
  dict.Set("host", NetLogStringValue(hostname));
  dict.Set("verify_flags", flags);
  dict.Set("crlset_sequence", NetLogNumberValue(crl_set->sequence()));
  if (crl_set->IsExpired())
    dict.Set("crlset_is_expired", true);

  return dict;
}

}  // namespace

#if !(BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(CHROME_ROOT_STORE_ONLY))
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateSystemVerifyProc(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    scoped_refptr<CRLSet> crl_set) {
#if BUILDFLAG(IS_ANDROID)
  return base::MakeRefCounted<CertVerifyProcAndroid>(
      std::move(cert_net_fetcher), std::move(crl_set));
#elif BUILDFLAG(IS_IOS)
  return base::MakeRefCounted<CertVerifyProcIOS>(std::move(crl_set));
#else
#error Unsupported platform
#endif
}
#endif

#if BUILDFLAG(IS_FUCHSIA)
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateBuiltinVerifyProc(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<CTVerifier> ct_verifier,
    scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
    const InstanceParams instance_params,
    std::optional<network_time::TimeTracker> time_tracker) {
  return CreateCertVerifyProcBuiltin(
      std::move(cert_net_fetcher), std::move(crl_set), std::move(ct_verifier),
      std::move(ct_policy_enforcer), CreateSslSystemTrustStore(),
      instance_params, std::move(time_tracker));
}
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateBuiltinWithChromeRootStore(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    scoped_refptr<CRLSet> crl_set,
    std::unique_ptr<CTVerifier> ct_verifier,
    scoped_refptr<CTPolicyEnforcer> ct_policy_enforcer,
    const ChromeRootStoreData* root_store_data,
    const InstanceParams instance_params,
    std::optional<network_time::TimeTracker> time_tracker) {
  std::unique_ptr<TrustStoreChrome> chrome_root =
      root_store_data ? std::make_unique<TrustStoreChrome>(*root_store_data)
                      : std::make_unique<TrustStoreChrome>();
  return CreateCertVerifyProcBuiltin(
      std::move(cert_net_fetcher), std::move(crl_set), std::move(ct_verifier),
      std::move(ct_policy_enforcer),
      CreateSslSystemTrustStoreChromeRoot(std::move(chrome_root)),
      instance_params, std::move(time_tracker));
}
#endif

CertVerifyProc::CertVerifyProc(scoped_refptr<CRLSet> crl_set)
    : crl_set_(std::move(crl_set)) {
  CHECK(crl_set_);
}

CertVerifyProc::~CertVerifyProc() = default;

int CertVerifyProc::Verify(X509Certificate* cert,
                           const std::string& hostname,
                           const std::string& ocsp_response,
                           const std::string& sct_list,
                           int flags,
                           CertVerifyResult* verify_result,
                           const NetLogWithSource& net_log) {
  CHECK(cert);
  CHECK(verify_result);

  net_log.BeginEvent(NetLogEventType::CERT_VERIFY_PROC, [&] {
    return CertVerifyParams(cert, hostname, ocsp_response, sct_list, flags,
                            crl_set());
  });
  // CertVerifyProc's contract allows ::VerifyInternal() to wait on File I/O
  // (such as the Windows registry or smart cards on all platforms) or may re-
  // enter this code via extension hooks (such as smart card UI). To ensure
  // threads are not starved or deadlocked, the base::ScopedBlockingCall below
  // increments the thread pool capacity when this method takes too much time to
  // run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  verify_result->Reset();
  verify_result->verified_cert = cert;

  int rv = VerifyInternal(cert, hostname, ocsp_response, sct_list, flags,
                          verify_result, net_log);

  CHECK(verify_result->verified_cert);

  // Check for mismatched signature algorithms and unknown signature algorithms
  // in the chain. Also fills in the has_* booleans for the digest algorithms
  // present in the chain.
  if (!InspectSignatureAlgorithmsInChain(verify_result)) {
    verify_result->cert_status |= CERT_STATUS_INVALID;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  if (!cert->VerifyNameMatch(hostname)) {
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  if (verify_result->ocsp_result.response_status ==
      bssl::OCSPVerifyResult::NOT_CHECKED) {
    // If VerifyInternal did not record the result of checking stapled OCSP,
    // do it now.
    BestEffortCheckOCSP(ocsp_response, *verify_result->verified_cert,
                        &verify_result->ocsp_result);
  }

  // Check to see if the connection is being intercepted.
  for (const auto& hash : verify_result->public_key_hashes) {
    if (hash.tag() != HASH_VALUE_SHA256) {
      continue;
    }
    if (!crl_set()->IsKnownInterceptionKey(std::string_view(
            reinterpret_cast<const char*>(hash.data()), hash.size()))) {
      continue;
    }

    if (verify_result->cert_status & CERT_STATUS_REVOKED) {
      // If the chain was revoked, and a known MITM was present, signal that
      // with a more meaningful error message.
      verify_result->cert_status |= CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED;
      rv = MapCertStatusToNetError(verify_result->cert_status);
    } else {
      // Otherwise, simply signal informatively. Both statuses are not set
      // simultaneously.
      verify_result->cert_status |= CERT_STATUS_KNOWN_INTERCEPTION_DETECTED;
    }
    break;
  }

  std::vector<std::string> dns_names, ip_addrs;
  cert->GetSubjectAltName(&dns_names, &ip_addrs);
  if (HasNameConstraintsViolation(verify_result->public_key_hashes,
                                  cert->subject().common_name,
                                  dns_names,
                                  ip_addrs)) {
    verify_result->cert_status |= CERT_STATUS_NAME_CONSTRAINT_VIOLATION;
    rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Check for weak keys in the entire verified chain.
  bool weak_key = ExaminePublicKeys(verify_result->verified_cert,
                                    verify_result->is_issued_by_known_root);

  if (weak_key) {
    verify_result->cert_status |= CERT_STATUS_WEAK_KEY;
    // Avoid replacing a more serious error, such as an OS/library failure,
    // by ensuring that if verification failed, it failed with a certificate
    // error.
    if (rv == OK || IsCertificateError(rv))
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  if (verify_result->has_sha1)
    verify_result->cert_status |= CERT_STATUS_SHA1_SIGNATURE_PRESENT;

  // Flag certificates using weak signature algorithms.
  bool sha1_allowed = (flags & VERIFY_ENABLE_SHA1_LOCAL_ANCHORS) &&
                      !verify_result->is_issued_by_known_root;
  if (!sha1_allowed && verify_result->has_sha1) {
    verify_result->cert_status |= CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
    // Avoid replacing a more serious error, such as an OS/library failure,
    // by ensuring that if verification failed, it failed with a certificate
    // error.
    if (rv == OK || IsCertificateError(rv))
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Distrust Symantec-issued certificates, as described at
  // https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
  if (!(flags & VERIFY_DISABLE_SYMANTEC_ENFORCEMENT) &&
      IsLegacySymantecCert(verify_result->public_key_hashes)) {
    verify_result->cert_status |= CERT_STATUS_SYMANTEC_LEGACY;
    if (rv == OK || IsCertificateError(rv))
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Flag certificates using too long validity periods.
  if (verify_result->is_issued_by_known_root && HasTooLongValidity(*cert)) {
    verify_result->cert_status |= CERT_STATUS_VALIDITY_TOO_LONG;
    if (rv == OK)
      rv = MapCertStatusToNetError(verify_result->cert_status);
  }

  // Flag certificates from publicly-trusted CAs that are issued to intranet
  // hosts. These are not allowed per the CA/Browser Forum requirements.
  //
  // Validity period is checked first just for testing convenience; there's not
  // a strong security reason to let validity period vs non-unique names take
  // precedence.
  if (verify_result->is_issued_by_known_root && IsHostnameNonUnique(hostname)) {
    verify_result->cert_status |= CERT_STATUS_NON_UNIQUE_NAME;
    // On Cronet, CERT_STATUS_NON_UNIQUE_NAME is recorded as a warning but not
    // treated as an error, because consumers have tests that use certs with
    // non-unique names. See b/337196170 (Google-internal).
#if !BUILDFLAG(CRONET_BUILD)
    if (rv == OK) {
      rv = MapCertStatusToNetError(verify_result->cert_status);
    }
#endif  // !BUILDFLAG(CRONET_BUILD)
  }

  // Record a histogram for per-verification usage of root certs.
  if (rv == OK) {
    RecordTrustAnchorHistogram(verify_result->public_key_hashes,
                               verify_result->is_issued_by_known_root);
  }

  net_log.EndEvent(NetLogEventType::CERT_VERIFY_PROC,
                   [&] { return verify_result->NetLogParams(rv); });
  return rv;
}

// static
void CertVerifyProc::LogNameNormalizationResult(
    const std::string& histogram_suffix,
    NameNormalizationResult result) {
  base::UmaHistogramEnumeration(
      std::string("Net.CertVerifier.NameNormalizationPrivateRoots") +
          histogram_suffix,
      result);
}

// static
void CertVerifyProc::LogNameNormalizationMetrics(
    const std::string& histogram_suffix,
    X509Certificate* verified_cert,
    bool is_issued_by_known_root) {
  if (is_issued_by_known_root)
    return;

  if (verified_cert->intermediate_buffers().empty()) {
    LogNameNormalizationResult(histogram_suffix,
                               NameNormalizationResult::kChainLengthOne);
    return;
  }

  std::vector<CRYPTO_BUFFER*> der_certs;
  der_certs.push_back(verified_cert->cert_buffer());
  for (const auto& buf : verified_cert->intermediate_buffers())
    der_certs.push_back(buf.get());

  bssl::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;

  std::vector<bssl::der::Input> subjects;
  std::vector<bssl::der::Input> issuers;

  for (auto* buf : der_certs) {
    bssl::der::Input tbs_certificate_tlv;
    bssl::der::Input signature_algorithm_tlv;
    bssl::der::BitString signature_value;
    bssl::ParsedTbsCertificate tbs;
    if (!bssl::ParseCertificate(
            bssl::der::Input(CRYPTO_BUFFER_data(buf), CRYPTO_BUFFER_len(buf)),
            &tbs_certificate_tlv, &signature_algorithm_tlv, &signature_value,
            nullptr /* errors*/) ||
        !ParseTbsCertificate(tbs_certificate_tlv, options, &tbs,
                             nullptr /*errors*/)) {
      LogNameNormalizationResult(histogram_suffix,
                                 NameNormalizationResult::kError);
      return;
    }
    subjects.push_back(tbs.subject_tlv);
    issuers.push_back(tbs.issuer_tlv);
  }

  for (size_t i = 0; i < subjects.size() - 1; ++i) {
    if (issuers[i] != subjects[i + 1]) {
      LogNameNormalizationResult(histogram_suffix,
                                 NameNormalizationResult::kNormalized);
      return;
    }
  }

  LogNameNormalizationResult(histogram_suffix,
                             NameNormalizationResult::kByteEqual);
}

// CheckNameConstraints verifies that every name in |dns_names| is in one of
// the domains specified by |domains|.
static bool CheckNameConstraints(const std::vector<std::string>& dns_names,
                                 base::span<const std::string_view> domains) {
  for (const auto& host : dns_names) {
    bool ok = false;
    url::CanonHostInfo host_info;
    const std::string dns_name = CanonicalizeHost(host, &host_info);
    if (host_info.IsIPAddress())
      continue;

    // If the name is not in a known TLD, ignore it. This permits internal
    // server names.
    if (!registry_controlled_domains::HostHasRegistryControlledDomain(
            dns_name, registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
            registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
      continue;
    }

    for (const auto& domain : domains) {
      // The |domain| must be of ".somesuffix" form, and |dns_name| must
      // have |domain| as a suffix.
      DCHECK_EQ('.', domain[0]);
      if (dns_name.size() <= domain.size())
        continue;
      std::string_view suffix =
          std::string_view(dns_name).substr(dns_name.size() - domain.size());
      if (!base::EqualsCaseInsensitiveASCII(suffix, domain))
        continue;
      ok = true;
      break;
    }

    if (!ok)
      return false;
  }

  return true;
}

// static
bool CertVerifyProc::HasNameConstraintsViolation(
    const HashValueVector& public_key_hashes,
    const std::string& common_name,
    const std::vector<std::string>& dns_names,
    const std::vector<std::string>& ip_addrs) {
  static constexpr std::string_view kDomainsANSSI[] = {
      ".fr",  // France
      ".gp",  // Guadeloupe
      ".gf",  // Guyane
      ".mq",  // Martinique
      ".re",  // Réunion
      ".yt",  // Mayotte
      ".pm",  // Saint-Pierre et Miquelon
      ".bl",  // Saint Barthélemy
      ".mf",  // Saint Martin
      ".wf",  // Wallis et Futuna
      ".pf",  // Polynésie française
      ".nc",  // Nouvelle Calédonie
      ".tf",  // Terres australes et antarctiques françaises
  };

  static constexpr std::string_view kDomainsTest[] = {
      ".example.com",
  };

  // PublicKeyDomainLimitation contains SHA-256(SPKI) and a pointer to an array
  // of fixed-length strings that contain the domains that the SPKI is allowed
  // to issue for.
  //
  // A public key hash can be generated with the following command:
  // openssl x509 -noout -in <cert>.pem -pubkey | \
  //   openssl asn1parse -noout -inform pem -out - | \
  //   openssl dgst -sha256 -binary | xxd -i
  static const struct PublicKeyDomainLimitation {
    SHA256HashValue public_key_hash;
    base::span<const std::string_view> domains;
  } kLimits[] = {
      // C=FR, ST=France, L=Paris, O=PM/SGDN, OU=DCSSI,
      // CN=IGC/A/emailAddress=igca@sgdn.pm.gouv.fr
      //
      // net/data/ssl/name_constrained/b9bea7860a962ea3611dab97ab6da3e21c1068b97d55575ed0e11279c11c8932.pem
      {
          {{0x86, 0xc1, 0x3a, 0x34, 0x08, 0xdd, 0x1a, 0xa7, 0x7e, 0xe8, 0xb6,
            0x94, 0x7c, 0x03, 0x95, 0x87, 0x72, 0xf5, 0x31, 0x24, 0x8c, 0x16,
            0x27, 0xbe, 0xfb, 0x2c, 0x4f, 0x4b, 0x04, 0xd0, 0x44, 0x96}},
          kDomainsANSSI,
      },
      // Not a real certificate - just for testing.
      // net/data/ssl/certificates/name_constrained_key.pem
      {
          {{0xa2, 0x2a, 0x88, 0x82, 0xba, 0x0c, 0xae, 0x9d, 0xf2, 0xc4, 0x5b,
            0x15, 0xa6, 0x1e, 0xfd, 0xfd, 0x19, 0x6b, 0xb1, 0x09, 0x19, 0xfd,
            0xac, 0x77, 0x9b, 0xd6, 0x08, 0x66, 0xda, 0xa8, 0xd2, 0x88}},
          kDomainsTest,
      },
  };

  for (const auto& limit : kLimits) {
    for (const auto& hash : public_key_hashes) {
      if (hash.tag() != HASH_VALUE_SHA256)
        continue;
      if (memcmp(hash.data(), limit.public_key_hash.data, hash.size()) != 0)
        continue;
      if (dns_names.empty() && ip_addrs.empty()) {
        std::vector<std::string> names;
        names.push_back(common_name);
        if (!CheckNameConstraints(names, limit.domains))
          return true;
      } else {
        if (!CheckNameConstraints(dns_names, limit.domains))
          return true;
      }
    }
  }

  return false;
}

// static
bool CertVerifyProc::HasTooLongValidity(const X509Certificate& cert) {
  const base::Time& start = cert.valid_start();
  const base::Time& expiry = cert.valid_expiry();
  if (start.is_max() || start.is_null() || expiry.is_max() ||
      expiry.is_null() || start > expiry) {
    return true;
  }

  // The maximum lifetime of publicly trusted certificates has reduced
  // gradually over time. These dates are derived from the transitions noted in
  // Section 1.2.2 (Relevant Dates) of the Baseline Requirements.
  //
  // * Certificates issued before BRs took effect, Chrome limited to max of ten
  // years validity and a max notAfter date of 2019-07-01.
  //   * Last possible expiry: 2019-07-01.
  //
  // * Cerificates issued on-or-after the BR effective date of 1 July 2012: 60
  // months.
  //   * Last possible expiry: 1 April 2015 + 60 months = 2020-04-01
  //
  // * Certificates issued on-or-after 1 April 2015: 39 months.
  //   * Last possible expiry: 1 March 2018 + 39 months = 2021-06-01
  //
  // * Certificates issued on-or-after 1 March 2018: 825 days.
  //   * Last possible expiry: 1 September 2020 + 825 days = 2022-12-05
  //
  // The current limit, from Chrome Root Certificate Policy:
  // * Certificates issued on-or-after 1 September 2020: 398 days.

  base::TimeDelta validity_duration = cert.valid_expiry() - cert.valid_start();

  // No certificates issued before the latest lifetime requirement was enacted
  // could possibly still be accepted, so we don't need to check the older
  // limits explicitly.
  return validity_duration > base::Days(398);
}

CertVerifyProc::ImplParams::ImplParams() {
  crl_set = net::CRLSet::BuiltinCRLSet();
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  // Defaults to using Chrome Root Store, though we have to keep this option in
  // here to allow WebView to turn this option off.
  use_chrome_root_store = true;
#endif
}

CertVerifyProc::ImplParams::~ImplParams() = default;

CertVerifyProc::ImplParams::ImplParams(const ImplParams&) = default;
CertVerifyProc::ImplParams& CertVerifyProc::ImplParams::operator=(
    const ImplParams& other) = default;
CertVerifyProc::ImplParams::ImplParams(ImplParams&&) = default;
CertVerifyProc::ImplParams& CertVerifyProc::ImplParams::operator=(
    ImplParams&& other) = default;

CertVerifyProc::InstanceParams::InstanceParams() = default;
CertVerifyProc::InstanceParams::~InstanceParams() = default;

CertVerifyProc::InstanceParams::InstanceParams(const InstanceParams&) = default;
CertVerifyProc::InstanceParams& CertVerifyProc::InstanceParams::operator=(
    const InstanceParams& other) = default;
CertVerifyProc::InstanceParams::InstanceParams(InstanceParams&&) = default;
CertVerifyProc::InstanceParams& CertVerifyProc::InstanceParams::operator=(
    InstanceParams&& other) = default;

CertVerifyProc::CertificateWithConstraints::CertificateWithConstraints() =
    default;
CertVerifyProc::CertificateWithConstraints::~CertificateWithConstraints() =
    default;

CertVerifyProc::CertificateWithConstraints::CertificateWithConstraints(
    const CertificateWithConstraints&) = default;
CertVerifyProc::CertificateWithConstraints&
CertVerifyProc::CertificateWithConstraints::operator=(
    const CertificateWithConstraints& other) = default;
CertVerifyProc::CertificateWithConstraints::CertificateWithConstraints(
    CertificateWithConstraints&&) = default;
CertVerifyProc::CertificateWithConstraints&
CertVerifyProc::CertificateWithConstraints::operator=(
    CertificateWithConstraints&& other) = default;

}  // namespace net
