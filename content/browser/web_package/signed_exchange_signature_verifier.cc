// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace content {

namespace {

// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#signature-validity
// Step 5. "Let message be the concatenation of the following byte strings."
constexpr uint8_t kMessageHeader[] =
    // 5.1. "A string that consists of octet 32 (0x20) repeated 64 times."
    // [spec text]
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
    // 5.2. "A context string: the ASCII encoding of "HTTP Exchange 1"." ...
    // "but implementations of drafts MUST NOT use it and MUST use another
    // draft-specific string beginning with "HTTP Exchange 1 " instead."
    // [spec text]
    // 5.3. "A single 0 byte which serves as a separator." [spec text]
    "HTTP Exchange 1 b2";

constexpr int kFourWeeksInSeconds = base::TimeDelta::FromDays(28).InSeconds();
constexpr int kOneWeekInSeconds = base::TimeDelta::FromDays(7).InSeconds();

base::Optional<crypto::SignatureVerifier::SignatureAlgorithm>
GetSignatureAlgorithm(scoped_refptr<net::X509Certificate> cert,
                      SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "VerifySignature");
  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
          &spki)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Failed to extract SPKI.");
    return base::nullopt;
  }

  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse public key.");
    return base::nullopt;
  }

  int pkey_id = EVP_PKEY_id(pkey.get());
  if (pkey_id != EVP_PKEY_EC) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Unsupported public key type: %d. Only ECDSA keys "
                           "on the secp256r1 curve are supported.",
                           pkey_id));
    return base::nullopt;
  }

  const EC_GROUP* group = EC_KEY_get0_group(EVP_PKEY_get0_EC_KEY(pkey.get()));
  int curve_name = EC_GROUP_get_curve_name(group);
  if (curve_name == NID_X9_62_prime256v1)
    return crypto::SignatureVerifier::ECDSA_SHA256;
  signed_exchange_utils::ReportErrorAndTraceEvent(
      devtools_proxy,
      base::StringPrintf("Unsupported EC group: %d. Only ECDSA keys on the "
                         "secp256r1 curve are supported.",
                         curve_name));
  return base::nullopt;
}

bool VerifySignature(base::span<const uint8_t> sig,
                     base::span<const uint8_t> msg,
                     scoped_refptr<net::X509Certificate> cert,
                     crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                     SignedExchangeDevToolsProxy* devtools_proxy) {
  crypto::SignatureVerifier verifier;
  if (!net::x509_util::SignatureVerifierInitWithCertificate(
          &verifier, algorithm, sig, cert->cert_buffer())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "SignatureVerifierInitWithCertificate failed.");
    return false;
  }
  verifier.VerifyUpdate(msg);
  if (!verifier.VerifyFinal()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "VerifyFinal failed.");
    return false;
  }
  return true;
}

std::string HexDump(const std::vector<uint8_t>& msg) {
  std::string output;
  for (const auto& byte : msg) {
    base::StringAppendF(&output, "%02x", byte);
  }
  return output;
}

void AppendToBuf8BytesBigEndian(std::vector<uint8_t>* buf, uint64_t n) {
  char encoded[8];
  base::WriteBigEndian(encoded, n);
  buf->insert(buf->end(), std::begin(encoded), std::end(encoded));
}

base::Optional<std::vector<uint8_t>> GenerateSignedMessage(
    const SignedExchangeEnvelope& envelope) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("loading"),
                     "GenerateSignedMessage");

  const auto signature = envelope.signature();

  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
  // Step 5. "Let message be the concatenation of the following byte strings."
  std::vector<uint8_t> message;
  // see kMessageHeader for Steps 5.1 to 5.3.
  message.insert(message.end(), std::begin(kMessageHeader),
                 std::end(kMessageHeader));

  // Step 5.4. "If cert-sha256 is set, a byte holding the value 32 followed by
  // the 32 bytes of the value of cert-sha256. Otherwise a 0 byte." [spec text]
  // Note: cert-sha256 must be set for application/signed-exchange envelope
  // format.
  message.push_back(32);
  const auto& cert_sha256 = envelope.signature().cert_sha256.value();
  message.insert(message.end(), std::begin(cert_sha256.data),
                 std::end(cert_sha256.data));

  // Step 5.5. "The 8-byte big-endian encoding of the length in bytes of
  // validity-url, followed by the bytes of validity-url." [spec text]
  const auto& validity_url_spec = signature.validity_url.spec();
  AppendToBuf8BytesBigEndian(&message, validity_url_spec.size());
  message.insert(message.end(), std::begin(validity_url_spec),
                 std::end(validity_url_spec));

  // Step 5.6. "The 8-byte big-endian encoding of date." [spec text]
  AppendToBuf8BytesBigEndian(&message, signature.date);

  // Step 5.7. "The 8-byte big-endian encoding of expires." [spec text]
  AppendToBuf8BytesBigEndian(&message, signature.expires);

  // Step 5.8. "The 8-byte big-endian encoding of the length in bytes of
  // requestUrl, followed by the bytes of requestUrl." [spec text]
  const auto& request_url_spec = envelope.request_url().spec();

  AppendToBuf8BytesBigEndian(&message, request_url_spec.size());
  message.insert(message.end(), std::begin(request_url_spec),
                 std::end(request_url_spec));

  // Step 5.9. "The 8-byte big-endian encoding of the length in bytes of
  // headers, followed by the bytes of headers." [spec text]
  AppendToBuf8BytesBigEndian(&message, envelope.cbor_header().size());
  message.insert(message.end(), envelope.cbor_header().begin(),
                 envelope.cbor_header().end());

  TRACE_EVENT_END1(TRACE_DISABLED_BY_DEFAULT("loading"),
                   "GenerateSignedMessage", "dump", HexDump(message));
  return message;
}

base::Time TimeFromSignedExchangeUnixTime(uint64_t t) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(t);
}

// Implements steps 3-4 of
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
bool VerifyTimestamps(const SignedExchangeEnvelope& envelope,
                      const base::Time& verification_time) {
  base::Time expires_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().expires);
  base::Time creation_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().date);

  // 3. "If expires is more than 7 days (604800 seconds) after date, return
  // "invalid"." [spec text]
  if ((expires_time - creation_time).InSeconds() > kOneWeekInSeconds)
    return false;

  // 4. "If the current time is before date or after expires, return
  // "invalid"."
  if (verification_time < creation_time) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "SignedExchange.SignatureVerificationError.NotYetValid",
        (creation_time - verification_time).InSeconds(), 1, kFourWeeksInSeconds,
        50);
    return false;
  }
  if (expires_time < verification_time) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "SignedExchange.SignatureVerificationError.Expired",
        (verification_time - expires_time).InSeconds(), 1, kFourWeeksInSeconds,
        50);
    return false;
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS("SignedExchange.TimeUntilExpiration",
                              (expires_time - verification_time).InSeconds(), 1,
                              kOneWeekInSeconds, 50);
  return true;
}

// Returns true if SPKI hash of |certificate| is included in the
// --ignore-certificate-errors-spki-list command line flag, and
// ContentBrowserClient::CanIgnoreCertificateErrorIfNeeded() returns true.
bool ShouldIgnoreTimestampError(
    scoped_refptr<net::X509Certificate> certificate) {
  static base::NoDestructor<
      SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList>
      instance(*base::CommandLine::ForCurrentProcess());
  return instance->ShouldIgnoreError(certificate);
}

}  // namespace

SignedExchangeSignatureVerifier::Result SignedExchangeSignatureVerifier::Verify(
    const SignedExchangeEnvelope& envelope,
    scoped_refptr<net::X509Certificate> certificate,
    const base::Time& verification_time,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  SCOPED_UMA_HISTOGRAM_TIMER("SignedExchange.Time.SignatureVerify");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeSignatureVerifier::Verify");

  if (!VerifyTimestamps(envelope, verification_time) &&
      !ShouldIgnoreTimestampError(certificate)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Invalid timestamp. creation_time: %" PRIu64
            ", expires_time: %" PRIu64 ", verification_time: %" PRIu64,
            envelope.signature().date, envelope.signature().expires,
            (verification_time - base::Time::UnixEpoch()).InSeconds()));
    return Result::kErrInvalidTimestamp;
  }

  if (!certificate) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "No certificate set.");
    return Result::kErrNoCertificate;
  }

  if (!envelope.signature().cert_sha256.has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "No cert-sha256 set.");
    return Result::kErrNoCertificateSHA256;
  }

  // The main-certificate is the first certificate in certificate-chain.
  if (*envelope.signature().cert_sha256 !=
      net::X509Certificate::CalculateFingerprint256(
          certificate->cert_buffer())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "cert-sha256 mismatch.");
    return Result::kErrCertificateSHA256Mismatch;
  }

  auto message = GenerateSignedMessage(envelope);
  if (!message) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to reconstruct signed message.");
    return Result::kErrInvalidSignatureFormat;
  }

  base::Optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
      GetSignatureAlgorithm(certificate, devtools_proxy);
  if (!algorithm)
    return Result::kErrUnsupportedCertType;

  const std::string& sig = envelope.signature().sig;
  if (!VerifySignature(
          base::make_span(reinterpret_cast<const uint8_t*>(sig.data()),
                          sig.size()),
          *message, certificate, *algorithm, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to verify signature \"sig\".");
    return Result::kErrSignatureVerificationFailed;
  }

  if (!base::EqualsCaseInsensitiveASCII(envelope.signature().integrity,
                                        "digest/mi-sha256-03")) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "The current implemention only supports \"digest/mi-sha256-03\" "
        "integrity scheme.");
    return Result::kErrInvalidSignatureIntegrity;
  }
  return Result::kSuccess;
}

SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList::IgnoreErrorsSPKIList(
    const std::string& spki_list) {
  Parse(spki_list);
}

SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList::IgnoreErrorsSPKIList(
    const base::CommandLine& command_line) {
  if (!GetContentClient()->browser()->CanIgnoreCertificateErrorIfNeeded())
    return;
  Parse(command_line.GetSwitchValueASCII(
      network::switches::kIgnoreCertificateErrorsSPKIList));
}

void SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList::Parse(
    const std::string& spki_list) {
  hash_set_ =
      network::IgnoreErrorsCertVerifier::MakeWhitelist(base::SplitString(
          spki_list, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL));
}

SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList::~IgnoreErrorsSPKIList() =
    default;

bool SignedExchangeSignatureVerifier::IgnoreErrorsSPKIList::ShouldIgnoreError(
    scoped_refptr<net::X509Certificate> certificate) {
  if (hash_set_.empty())
    return false;

  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate->cert_buffer()),
          &spki)) {
    return false;
  }
  net::SHA256HashValue hash;
  crypto::SHA256HashString(spki, &hash, sizeof(net::SHA256HashValue));
  return hash_set_.find(hash) != hash_set_.end();
}

}  // namespace content
