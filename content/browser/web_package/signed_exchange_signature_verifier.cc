// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_verifier.h"

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/format_macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_certificate_chain.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_envelope.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/signature_verifier.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
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
    "HTTP Exchange 1 b3";

constexpr base::TimeDelta kOneWeek = base::Days(7);
constexpr base::TimeDelta kFourWeeks = base::Days(4 * 7);

std::optional<crypto::SignatureVerifier::SignatureAlgorithm>
GetSignatureAlgorithm(scoped_refptr<net::X509Certificate> cert,
                      SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "GetSignatureAlgorithm");
  std::string_view spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
          &spki)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "Failed to extract SPKI.");
    return std::nullopt;
  }

  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(spki.data()), spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (!pkey || CBS_len(&cbs) != 0) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse public key.");
    return std::nullopt;
  }

  int pkey_id = EVP_PKEY_id(pkey.get());
  if (pkey_id != EVP_PKEY_EC) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Unsupported public key type: %d. Only ECDSA keys "
                           "on the secp256r1 curve are supported.",
                           pkey_id));
    return std::nullopt;
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
  return std::nullopt;
}

bool VerifySignature(base::span<const uint8_t> sig,
                     base::span<const uint8_t> msg,
                     scoped_refptr<net::X509Certificate> cert,
                     crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                     SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "VerifySignature");
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
  std::array<uint8_t, 8> encoded = base::U64ToBigEndian(n);
  buf->insert(buf->end(), encoded.begin(), encoded.end());
}

std::vector<uint8_t> GenerateSignedMessage(
    SignedExchangeVersion version,
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
  const auto& validity_url_bytes = signature.validity_url.raw_string;
  AppendToBuf8BytesBigEndian(&message, validity_url_bytes.size());
  message.insert(message.end(), std::begin(validity_url_bytes),
                 std::end(validity_url_bytes));

  // Step 5.6. "The 8-byte big-endian encoding of date." [spec text]
  AppendToBuf8BytesBigEndian(&message, signature.date);

  // Step 5.7. "The 8-byte big-endian encoding of expires." [spec text]
  AppendToBuf8BytesBigEndian(&message, signature.expires);

  // Step 5.8. "The 8-byte big-endian encoding of the length in bytes of
  // requestUrl, followed by the bytes of requestUrl." [spec text]
  const auto& request_url_bytes = envelope.request_url().raw_string;

  AppendToBuf8BytesBigEndian(&message, request_url_bytes.size());
  message.insert(message.end(), std::begin(request_url_bytes),
                 std::end(request_url_bytes));

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
  return base::Time::UnixEpoch() + base::Seconds(t);
}

SignedExchangeSignatureVerifier::Result VerifyValidityPeriod(
    const SignedExchangeEnvelope& envelope) {
  base::Time expires_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().expires);
  base::Time creation_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().date);

  // 3. "If expires is more than 7 days (604800 seconds) after date, return
  // "invalid"." [spec text]
  if ((expires_time - creation_time).InSeconds() > kOneWeek.InSeconds()) {
    return SignedExchangeSignatureVerifier::Result::kErrValidityPeriodTooLong;
  }
  return SignedExchangeSignatureVerifier::Result::kSuccess;
}

// Implements "Signature validity" of
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-validity
SignedExchangeSignatureVerifier::Result VerifyTimestamps(
    const SignedExchangeEnvelope& envelope,
    const base::Time& verification_time) {
  base::Time expires_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().expires);
  base::Time creation_time =
      TimeFromSignedExchangeUnixTime(envelope.signature().date);

  // 4. "If the current time is before date or after expires, return
  // "invalid"."
  if (verification_time < creation_time) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "SignedExchange.SignatureVerificationError.NotYetValid",
        (creation_time - verification_time).InSeconds(), 1,
        kFourWeeks.InSeconds(), 50);
    return SignedExchangeSignatureVerifier::Result::kErrFutureDate;
  }
  if (expires_time < verification_time) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "SignedExchange.SignatureVerificationError.Expired",
        (verification_time - expires_time).InSeconds(), 1,
        kFourWeeks.InSeconds(), 50);
    return SignedExchangeSignatureVerifier::Result::kErrExpired;
  }

  UMA_HISTOGRAM_CUSTOM_COUNTS("SignedExchange.TimeUntilExpiration",
                              (expires_time - verification_time).InSeconds(), 1,
                              kOneWeek.InSeconds(), 50);
  return SignedExchangeSignatureVerifier::Result::kSuccess;
}

}  // namespace

SignedExchangeSignatureVerifier::Result SignedExchangeSignatureVerifier::Verify(
    SignedExchangeVersion version,
    const SignedExchangeEnvelope& envelope,
    const SignedExchangeCertificateChain* cert_chain,
    const base::Time& verification_time,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  SCOPED_UMA_HISTOGRAM_TIMER("SignedExchange.Time.SignatureVerify");
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeSignatureVerifier::Verify");
  scoped_refptr<net::X509Certificate> certificate = cert_chain->cert();
  DCHECK(certificate);
  const auto validity_period_result = VerifyValidityPeriod(envelope);
  if (validity_period_result != Result::kSuccess) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Specified validity period too long. creation_time: %" PRIu64
            ", expires_time: %" PRIu64 ", verification_time: %" PRIu64,
            envelope.signature().date, envelope.signature().expires,
            (verification_time - base::Time::UnixEpoch()).InSeconds()));
    return validity_period_result;
  }
  const auto timestamp_result = VerifyTimestamps(envelope, verification_time);
  if (timestamp_result != Result::kSuccess &&
      !cert_chain->ShouldIgnoreErrors()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Invalid timestamp. creation_time: %" PRIu64
            ", expires_time: %" PRIu64 ", verification_time: %" PRIu64,
            envelope.signature().date, envelope.signature().expires,
            (verification_time - base::Time::UnixEpoch()).InSeconds()));
    return timestamp_result;
  }
  // Currently we don't support ed25519key. So |cert_sha256| must be set.
  DCHECK(envelope.signature().cert_sha256.has_value());

  // The main-certificate is the first certificate in certificate-chain.
  if (*envelope.signature().cert_sha256 !=
      net::X509Certificate::CalculateFingerprint256(
          certificate->cert_buffer())) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    "cert-sha256 mismatch.");
    return Result::kErrCertificateSHA256Mismatch;
  }

  auto message = GenerateSignedMessage(version, envelope);

  std::optional<crypto::SignatureVerifier::SignatureAlgorithm> algorithm =
      GetSignatureAlgorithm(certificate, devtools_proxy);
  if (!algorithm)
    return Result::kErrUnsupportedCertType;

  const std::string& sig = envelope.signature().sig;
  if (!VerifySignature(base::as_byte_span(sig), message, certificate,
                       *algorithm, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to verify signature \"sig\".");
    return Result::kErrSignatureVerificationFailed;
  }
  return Result::kSuccess;
}

}  // namespace content
