// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <stdint.h>

#include <algorithm>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/crypto_buildflags.h"
#include "crypto/sha2.h"
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
#include "net/cert/ocsp_revocation_status.h"
#include "net/cert/pem.h"
#include "net/cert/pki/extended_key_usage.h"
#include "net/cert/pki/ocsp.h"
#include "net/cert/pki/parse_certificate.h"
#include "net/cert/pki/signature_algorithm.h"
#include "net/cert/symantec_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(USE_NSS_CERTS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/cert_verify_proc_builtin.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#endif  // CHROME_ROOT_STORE_SUPPORTED

#if BUILDFLAG(IS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#elif BUILDFLAG(IS_IOS)
#include "net/cert/cert_verify_proc_ios.h"
#elif BUILDFLAG(IS_MAC)
#include "net/cert/cert_verify_proc_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "net/cert/cert_verify_proc_win.h"
#endif

namespace net {

namespace {

// Constants used to build histogram names
const char kLeafCert[] = "Leaf";
const char kIntermediateCert[] = "Intermediate";
const char kRootCert[] = "Root";

// Histogram buckets for RSA/DSA/DH key sizes.
const int kRsaDsaKeySizes[] = {512, 768, 1024, 1536, 2048, 3072, 4096, 8192,
                               16384};
// Histogram buckets for ECDSA/ECDH key sizes. The list is based upon the FIPS
// 186-4 approved curves.
const int kEccKeySizes[] = {163, 192, 224, 233, 256, 283, 384, 409, 521, 571};

const char* CertTypeToString(X509Certificate::PublicKeyType cert_type) {
  switch (cert_type) {
    case X509Certificate::kPublicKeyTypeUnknown:
      return "Unknown";
    case X509Certificate::kPublicKeyTypeRSA:
      return "RSA";
    case X509Certificate::kPublicKeyTypeDSA:
      return "DSA";
    case X509Certificate::kPublicKeyTypeECDSA:
      return "ECDSA";
    case X509Certificate::kPublicKeyTypeDH:
      return "DH";
    case X509Certificate::kPublicKeyTypeECDH:
      return "ECDH";
  }
  NOTREACHED();
  return "Unsupported";
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
  if (cert_type == X509Certificate::kPublicKeyTypeECDH ||
      cert_type == X509Certificate::kPublicKeyTypeECDSA) {
    // Typical key sizes match SECP/FIPS 186-3 recommendations for prime and
    // binary curves - which range from 163 bits to 571 bits.
    counter = base::CustomHistogram::FactoryGet(
        histogram_name,
        base::CustomHistogram::ArrayToCustomEnumRanges(kEccKeySizes),
        base::HistogramBase::kUmaTargetedHistogramFlag);
  } else {
    // Key sizes < 1024 bits should cause errors, while key sizes > 16K are not
    // uniformly supported by the underlying cryptographic libraries.
    counter = base::CustomHistogram::FactoryGet(
        histogram_name,
        base::CustomHistogram::ArrayToCustomEnumRanges(kRsaDsaKeySizes),
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }
  counter->Add(size_bits);
}

// Returns true if |type| is |kPublicKeyTypeRSA| or |kPublicKeyTypeDSA|, and
// if |size_bits| is < 1024. Note that this means there may be false
// negatives: keys for other algorithms and which are weak will pass this
// test.
bool IsWeakKey(X509Certificate::PublicKeyType type, size_t size_bits) {
  switch (type) {
    case X509Certificate::kPublicKeyTypeRSA:
    case X509Certificate::kPublicKeyTypeDSA:
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
                         OCSPVerifyResult* verify_result) {
  if (raw_response.empty()) {
    *verify_result = OCSPVerifyResult();
    verify_result->response_status = OCSPVerifyResult::MISSING;
    return;
  }

  base::StringPiece cert_der =
      x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer());

  // Try to get the certificate that signed |certificate|. This will run into
  // problems if the CertVerifyProc implementation doesn't return the ordered
  // certificates. If that happens the OCSP verification may be incorrect.
  base::StringPiece issuer_der;
  if (certificate.intermediate_buffers().empty()) {
    if (X509Certificate::IsSelfSigned(certificate.cert_buffer())) {
      issuer_der = cert_der;
    } else {
      // A valid cert chain wasn't provided.
      *verify_result = OCSPVerifyResult();
      return;
    }
  } else {
    issuer_der = x509_util::CryptoBufferAsStringPiece(
        certificate.intermediate_buffers().front().get());
  }

  verify_result->revocation_status = CheckOCSP(
      raw_response, std::string_view(cert_der.data(), cert_der.size()),
      std::string_view(issuer_der.data(), issuer_der.size()),
      base::Time::Now().ToTimeT(), kMaxRevocationLeafUpdateAge.InSeconds(),
      &verify_result->response_status);
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
  base::StringPiece cert_algorithm_sequence;
  base::StringPiece tbs_algorithm_sequence;

  // Extract the AlgorithmIdentifier SEQUENCEs
  if (!asn1::ExtractSignatureAlgorithmsFromDERCert(
          x509_util::CryptoBufferAsStringPiece(cert), &cert_algorithm_sequence,
          &tbs_algorithm_sequence)) {
    return false;
  }

  absl::optional<SignatureAlgorithm> cert_algorithm =
      ParseSignatureAlgorithm(der::Input(cert_algorithm_sequence));
  absl::optional<SignatureAlgorithm> tbs_algorithm =
      ParseSignatureAlgorithm(der::Input(tbs_algorithm_sequence));
  if (!cert_algorithm || !tbs_algorithm || *cert_algorithm != *tbs_algorithm) {
    return false;
  }

  switch (*cert_algorithm) {
    case SignatureAlgorithm::kRsaPkcs1Sha1:
    case SignatureAlgorithm::kEcdsaSha1:
      verify_result->has_sha1 = true;
      return true;  // For now.

    case SignatureAlgorithm::kRsaPkcs1Sha256:
    case SignatureAlgorithm::kRsaPkcs1Sha384:
    case SignatureAlgorithm::kRsaPkcs1Sha512:
    case SignatureAlgorithm::kEcdsaSha256:
    case SignatureAlgorithm::kEcdsaSha384:
    case SignatureAlgorithm::kEcdsaSha512:
    case SignatureAlgorithm::kRsaPssSha256:
    case SignatureAlgorithm::kRsaPssSha384:
    case SignatureAlgorithm::kRsaPssSha512:
      return true;
  }

  NOTREACHED();
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

base::Value CertVerifyParams(X509Certificate* cert,
                             const std::string& hostname,
                             const std::string& ocsp_response,
                             const std::string& sct_list,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors) {
  base::Value::Dict dict;
  dict.Set("certificates", NetLogX509CertificateList(cert));
  if (!ocsp_response.empty()) {
    dict.Set("ocsp_response", PEMEncode(ocsp_response, "NETLOG OCSP RESPONSE"));
  }
  if (!sct_list.empty()) {
    dict.Set("sct_list", PEMEncode(sct_list, "NETLOG SCT LIST"));
  }
  dict.Set("host", NetLogStringValue(hostname));
  dict.Set("verify_flags", flags);
  dict.Set("crlset_sequence", NetLogNumberValue(crl_set->sequence()));
  if (crl_set->IsExpired())
    dict.Set("crlset_is_expired", true);

  if (!additional_trust_anchors.empty()) {
    base::Value::List certs;
    for (auto& anchor : additional_trust_anchors) {
      std::string pem_encoded;
      if (X509Certificate::GetPEMEncodedFromDER(
              x509_util::CryptoBufferAsStringPiece(anchor->cert_buffer()),
              &pem_encoded)) {
        certs.Append(std::move(pem_encoded));
      }
    }
    dict.Set("additional_trust_anchors", std::move(certs));
  }

  return base::Value(std::move(dict));
}

}  // namespace

#if !(BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateSystemVerifyProc(
    scoped_refptr<CertNetFetcher> cert_net_fetcher) {
#if BUILDFLAG(IS_ANDROID)
  return base::MakeRefCounted<CertVerifyProcAndroid>(
      std::move(cert_net_fetcher));
#elif BUILDFLAG(IS_IOS)
  return base::MakeRefCounted<CertVerifyProcIOS>();
#elif BUILDFLAG(IS_MAC)
  return base::MakeRefCounted<CertVerifyProcMac>();
#elif BUILDFLAG(IS_WIN)
  return base::MakeRefCounted<CertVerifyProcWin>();
#else
#error Unsupported platform
#endif
}
#endif

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(USE_NSS_CERTS)
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateBuiltinVerifyProc(
    scoped_refptr<CertNetFetcher> cert_net_fetcher) {
  return CreateCertVerifyProcBuiltin(std::move(cert_net_fetcher),
                                     CreateSslSystemTrustStore());
}
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
// static
scoped_refptr<CertVerifyProc> CertVerifyProc::CreateBuiltinWithChromeRootStore(
    scoped_refptr<CertNetFetcher> cert_net_fetcher) {
  return CreateCertVerifyProcBuiltin(
      std::move(cert_net_fetcher),
      CreateSslSystemTrustStoreChromeRoot(
          std::make_unique<net::TrustStoreChrome>()));
}
#endif

CertVerifyProc::CertVerifyProc() = default;

CertVerifyProc::~CertVerifyProc() = default;

int CertVerifyProc::Verify(X509Certificate* cert,
                           const std::string& hostname,
                           const std::string& ocsp_response,
                           const std::string& sct_list,
                           int flags,
                           CRLSet* crl_set,
                           const CertificateList& additional_trust_anchors,
                           CertVerifyResult* verify_result,
                           const NetLogWithSource& net_log) {
  net_log.BeginEvent(NetLogEventType::CERT_VERIFY_PROC, [&] {
    return CertVerifyParams(cert, hostname, ocsp_response, sct_list, flags,
                            crl_set, additional_trust_anchors);
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

  DCHECK(crl_set);
  int rv =
      VerifyInternal(cert, hostname, ocsp_response, sct_list, flags, crl_set,
                     additional_trust_anchors, verify_result, net_log);

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
      OCSPVerifyResult::NOT_CHECKED) {
    // If VerifyInternal did not record the result of checking stapled OCSP,
    // do it now.
    BestEffortCheckOCSP(ocsp_response, *verify_result->verified_cert,
                        &verify_result->ocsp_result);
  }

  // Check to see if the connection is being intercepted.
  if (crl_set) {
    for (const auto& hash : verify_result->public_key_hashes) {
      if (hash.tag() != HASH_VALUE_SHA256)
        continue;
      if (!crl_set->IsKnownInterceptionKey(base::StringPiece(
              reinterpret_cast<const char*>(hash.data()), hash.size())))
        continue;

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

  // Flag certificates from publicly-trusted CAs that are issued to intranet
  // hosts. While the CA/Browser Forum Baseline Requirements (v1.1) permit
  // these to be issued until 1 November 2015, they represent a real risk for
  // the deployment of gTLDs and are being phased out ahead of the hard
  // deadline.
  if (verify_result->is_issued_by_known_root && IsHostnameNonUnique(hostname)) {
    verify_result->cert_status |= CERT_STATUS_NON_UNIQUE_NAME;
    // CERT_STATUS_NON_UNIQUE_NAME will eventually become a hard error. For
    // now treat it as a warning and do not map it to an error return value.
  }

  // Flag certificates using too long validity periods.
  if (verify_result->is_issued_by_known_root && HasTooLongValidity(*cert)) {
    verify_result->cert_status |= CERT_STATUS_VALIDITY_TOO_LONG;
    if (rv == OK)
      rv = MapCertStatusToNetError(verify_result->cert_status);
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

  ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;

  std::vector<der::Input> subjects;
  std::vector<der::Input> issuers;

  for (auto* buf : der_certs) {
    der::Input tbs_certificate_tlv;
    der::Input signature_algorithm_tlv;
    der::BitString signature_value;
    ParsedTbsCertificate tbs;
    if (!ParseCertificate(
            der::Input(CRYPTO_BUFFER_data(buf), CRYPTO_BUFFER_len(buf)),
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
                                 base::span<const base::StringPiece> domains) {
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
      base::StringPiece suffix =
          base::StringPiece(dns_name).substr(dns_name.size() - domain.size());
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
  static constexpr base::StringPiece kDomainsANSSI[] = {
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

  static constexpr base::StringPiece kDomainsIndiaCCA[] = {
      ".gov.in",   ".nic.in",    ".ac.in", ".rbi.org.in", ".bankofindia.co.in",
      ".ncode.in", ".tcs.co.in",
  };

  static constexpr base::StringPiece kDomainsTest[] = {
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
    base::span<const base::StringPiece> domains;
  } kLimits[] = {
      // C=FR, ST=France, L=Paris, O=PM/SGDN, OU=DCSSI,
      // CN=IGC/A/emailAddress=igca@sgdn.pm.gouv.fr
      //
      // net/data/ssl/blocklist/b9bea7860a962ea3611dab97ab6da3e21c1068b97d55575ed0e11279c11c8932.pem
      {
          {{0x86, 0xc1, 0x3a, 0x34, 0x08, 0xdd, 0x1a, 0xa7, 0x7e, 0xe8, 0xb6,
            0x94, 0x7c, 0x03, 0x95, 0x87, 0x72, 0xf5, 0x31, 0x24, 0x8c, 0x16,
            0x27, 0xbe, 0xfb, 0x2c, 0x4f, 0x4b, 0x04, 0xd0, 0x44, 0x96}},
          kDomainsANSSI,
      },
      // C=IN, O=India PKI, CN=CCA India 2007
      // Expires: July 4th 2015.
      //
      // net/data/ssl/blocklist/f375e2f77a108bacc4234894a9af308edeca1acd8fbde0e7aaa9634e9daf7e1c.pem
      {
          {{0x7e, 0x6a, 0xcd, 0x85, 0x3c, 0xac, 0xc6, 0x93, 0x2e, 0x9b, 0x51,
            0x9f, 0xda, 0xd1, 0xbe, 0xb5, 0x15, 0xed, 0x2a, 0x2d, 0x00, 0x25,
            0xcf, 0xd3, 0x98, 0xc3, 0xac, 0x1f, 0x0d, 0xbb, 0x75, 0x4b}},
          kDomainsIndiaCCA,
      },
      // C=IN, O=India PKI, CN=CCA India 2011
      // Expires: March 11 2016.
      //
      // net/data/ssl/blocklist/2d66a702ae81ba03af8cff55ab318afa919039d9f31b4d64388680f81311b65a.pem
      {
          {{0x42, 0xa7, 0x09, 0x84, 0xff, 0xd3, 0x99, 0xc4, 0xea, 0xf0, 0xe7,
            0x02, 0xa4, 0x4b, 0xef, 0x2a, 0xd8, 0xa7, 0x9b, 0x8b, 0xf4, 0x64,
            0x8f, 0x6b, 0xb2, 0x10, 0xe1, 0x23, 0xfd, 0x07, 0x57, 0x93}},
          kDomainsIndiaCCA,
      },
      // C=IN, O=India PKI, CN=CCA India 2014
      // Expires: March 5 2024.
      //
      // net/data/ssl/blocklist/60109bc6c38328598a112c7a25e38b0f23e5a7511cb815fb64e0c4ff05db7df7.pem
      {
          {{0x9c, 0xf4, 0x70, 0x4f, 0x3e, 0xe5, 0xa5, 0x98, 0x94, 0xb1, 0x6b,
            0xf0, 0x0c, 0xfe, 0x73, 0xd5, 0x88, 0xda, 0xe2, 0x69, 0xf5, 0x1d,
            0xe6, 0x6a, 0x4b, 0xa7, 0x74, 0x46, 0xee, 0x2b, 0xd1, 0xf7}},
          kDomainsIndiaCCA,
      },
      // Not a real certificate - just for testing.
      // net/data/ssl/certificates/name_constraint_*.pem
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

  // These dates are derived from the transitions noted in Section 1.2.2
  // (Relevant Dates) of the Baseline Requirements.
  const base::Time time_2012_07_01 =
      base::Time::UnixEpoch() + base::Seconds(1341100800);
  const base::Time time_2015_04_01 =
      base::Time::UnixEpoch() + base::Seconds(1427846400);
  const base::Time time_2018_03_01 =
      base::Time::UnixEpoch() + base::Seconds(1519862400);
  const base::Time time_2019_07_01 =
      base::Time::UnixEpoch() + base::Seconds(1561939200);
  // From Chrome Root Certificate Policy
  const base::Time time_2020_09_01 =
      base::Time::UnixEpoch() + base::Seconds(1598918400);

  // Compute the maximally permissive interpretations, accounting for leap
  // years.
  // 10 years - two possible leap years.
  constexpr base::TimeDelta kTenYears = base::Days((365 * 8) + (366 * 2));
  // 5 years - two possible leap years (year 0/year 4 or year 1/year 5).
  constexpr base::TimeDelta kSixtyMonths = base::Days((365 * 3) + (366 * 2));
  // 39 months - one possible leap year, two at 365 days, and the longest
  // monthly sequence of 31/31/30 days (June/July/August).
  constexpr base::TimeDelta kThirtyNineMonths =
      base::Days(366 + 365 + 365 + 31 + 31 + 30);

  base::TimeDelta validity_duration = cert.valid_expiry() - cert.valid_start();

  // For certificates issued before the BRs took effect.
  if (start < time_2012_07_01 &&
      (validity_duration > kTenYears || expiry > time_2019_07_01)) {
    return true;
  }

  // For certificates issued on-or-after the BR effective date of 1 July 2012:
  // 60 months.
  if (start >= time_2012_07_01 && validity_duration > kSixtyMonths)
    return true;

  // For certificates issued on-or-after 1 April 2015: 39 months.
  if (start >= time_2015_04_01 && validity_duration > kThirtyNineMonths)
    return true;

  // For certificates issued on-or-after 1 March 2018: 825 days.
  if (start >= time_2018_03_01 && validity_duration > base::Days(825)) {
    return true;
  }

  // For certificates issued on-or-after 1 September 2020: 398 days.
  if (start >= time_2020_09_01 && validity_duration > base::Days(398)) {
    return true;
  }

  return false;
}

}  // namespace net
