// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_header_field.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "crypto/sha2.h"
#include "net/http/structured_headers.h"

namespace content {

// static
absl::optional<std::vector<SignedExchangeSignatureHeaderField::Signature>>
SignedExchangeSignatureHeaderField::ParseSignature(
    base::StringPiece signature_str,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeSignatureHeaderField::ParseSignature");

  absl::optional<net::structured_headers::ParameterisedList> values =
      net::structured_headers::ParseParameterisedList(signature_str);
  if (!values) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse signature header.");
    return absl::nullopt;
  }

  std::vector<Signature> signatures;
  signatures.reserve(values->size());
  for (auto& value : *values) {
    if (!value.identifier.is_token()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse signature header.");
      return absl::nullopt;
    }
    signatures.push_back(Signature());
    Signature& sig = signatures.back();
    sig.label = value.identifier.GetString();

    const auto& sig_item = value.params[kSig];
    if (!sig_item.is_byte_sequence()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse 'sig' parameter.");
      return absl::nullopt;
    }
    sig.sig = sig_item.GetString();
    if (sig.sig.empty()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'sig' parameter is not set,");
      return absl::nullopt;
    }

    const auto& integrity_item = value.params[kIntegrity];
    if (!integrity_item.is_string()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse 'integrity' parameter.");
      return absl::nullopt;
    }
    sig.integrity = integrity_item.GetString();
    if (sig.integrity.empty()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'integrity' parameter is not set.");
      return absl::nullopt;
    }

    const auto& cert_url_item = value.params[kCertUrl];
    if (!cert_url_item.is_string()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse 'cert-url' parameter.");
      return absl::nullopt;
    }
    sig.cert_url = GURL(cert_url_item.GetString());
    if (!sig.cert_url.is_valid() || sig.cert_url.has_ref()) {
      // TODO(https://crbug.com/819467) : When we will support "ed25519Key", the
      // params may not have "cert-url".
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'cert-url' parameter is not a valid URL.");
      return absl::nullopt;
    }
    if (!sig.cert_url.SchemeIs("https") && !sig.cert_url.SchemeIs("data")) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'cert-url' should have 'https' or 'data' scheme.");
      return absl::nullopt;
    }

    const auto& cert_sha256_item = value.params[kCertSha256Key];
    if (!cert_sha256_item.is_byte_sequence()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse 'cert-sha256' parameter.");
      return absl::nullopt;
    }
    const std::string& cert_sha256_string = cert_sha256_item.GetString();
    if (cert_sha256_string.size() != crypto::kSHA256Length) {
      // TODO(https://crbug.com/819467) : When we will support "ed25519Key", the
      // params may not have "cert-sha256".
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'cert-sha256' parameter is not a SHA-256 digest.");
      return absl::nullopt;
    }
    net::SHA256HashValue cert_sha256;
    memcpy(&cert_sha256.data, cert_sha256_string.data(), crypto::kSHA256Length);
    sig.cert_sha256 = std::move(cert_sha256);

    // TODO(https://crbug.com/819467): Support ed25519key.
    // sig.ed25519_key = value.params["ed25519Key"];

    const auto& validity_url_item = value.params[kValidityUrlKey];
    if (!validity_url_item.is_string()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Failed to parse 'validity-url' parameter.");
      return absl::nullopt;
    }
    sig.validity_url =
        signed_exchange_utils::URLWithRawString(validity_url_item.GetString());
    if (!sig.validity_url.url.is_valid()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'validity-url' parameter is not a valid URL.");
      return absl::nullopt;
    }
    if (sig.validity_url.url.has_ref()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'validity-url' parameter can't have a fragment.");
      return absl::nullopt;
    }
    if (!sig.validity_url.url.SchemeIs("https")) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'validity-url' should have 'https' scheme.");
      return absl::nullopt;
    }

    const auto& date_item = value.params[kDateKey];
    if (!date_item.is_integer()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'date' parameter is not a number.");
      return absl::nullopt;
    }
    sig.date = date_item.GetInteger();

    const auto& expires_item = value.params[kExpiresKey];
    if (!expires_item.is_integer()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "'expires' parameter is not a number.");
      return absl::nullopt;
    }
    sig.expires = expires_item.GetInteger();
  }
  return signatures;
}

SignedExchangeSignatureHeaderField::Signature::Signature() = default;
SignedExchangeSignatureHeaderField::Signature::Signature(
    const Signature& other) = default;
SignedExchangeSignatureHeaderField::Signature::~Signature() = default;

}  // namespace content
