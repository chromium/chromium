// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_certificate_chain.h"

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/cbor/reader.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "net/cert/x509_certificate.h"

namespace content {

namespace {

// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#cert-chain-format
std::unique_ptr<SignedExchangeCertificateChain> ParseB2(
    base::span<const uint8_t> message,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeCertificateChain::ParseB2");

  cbor::Reader::DecoderError error;
  base::Optional<cbor::Value> value = cbor::Reader::Read(message, &error);
  if (!value.has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Failed to decode Value. CBOR error: %s",
                           cbor::Reader::ErrorCodeToString(error)));
    return nullptr;
  }
  if (!value->is_array()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected top-level Value to be an array. Actual type: %d",
            static_cast<int>(value->type())));
    return nullptr;
  }

  const cbor::Value::ArrayValue& top_level_array = value->GetArray();
  // Expect at least 2 elements (magic string and main certificate).
  if (top_level_array.size() < 2) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected top-level array to have at least 2 elements."
            "Actual element count: %" PRIuS,
            top_level_array.size()));
    return nullptr;
  }
  if (!top_level_array[0].is_string() ||
      top_level_array[0].GetString() != kCertChainCborMagic) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        "First element of cert chain CBOR does not match the magic string.");
    return nullptr;
  }

  std::vector<base::StringPiece> der_certs;
  der_certs.reserve(top_level_array.size() - 1);
  std::string ocsp;
  std::string sct;

  for (size_t i = 1; i < top_level_array.size(); i++) {
    if (!top_level_array[i].is_map()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Expected certificate map, got non-map type at index %zu."
              " Actual type: %d",
              i, static_cast<int>(top_level_array[i].type())));
      return nullptr;
    }
    const cbor::Value::MapValue& cert_map = top_level_array[i].GetMap();

    // Step 1. Each cert value MUST be a DER-encoded X.509v3 certificate
    // ([RFC5280]). Other key/value pairs in the same array item define
    // properties of this certificate. [spec text]
    auto cert_iter = cert_map.find(cbor::Value(kCertKey));
    if (cert_iter == cert_map.end() || !cert_iter->second.is_bytestring()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "cert is not found or not a bytestring, at index %zu.", i));
      return nullptr;
    }
    der_certs.push_back(cert_iter->second.GetBytestringAsString());

    auto ocsp_iter = cert_map.find(cbor::Value(kOcspKey));
    if (i == 1) {
      // Step 2. The first certificate’s ocsp value MUST be a complete,
      // DER-encoded OCSP response for that certificate (using the ASN.1 type
      // OCSPResponse defined in [RFC6960]). ... [spec text]
      if (ocsp_iter == cert_map.end() || !ocsp_iter->second.is_bytestring()) {
        signed_exchange_utils::ReportErrorAndTraceEvent(
            devtools_proxy,
            "ocsp is not a bytestring, or not found in the first cert map.");
        return nullptr;
      }
      ocsp = ocsp_iter->second.GetBytestringAsString().as_string();
      if (ocsp.empty()) {
        signed_exchange_utils::ReportErrorAndTraceEvent(
            devtools_proxy, "ocsp must not be empty.");
        return nullptr;
      }
    } else if (ocsp_iter != cert_map.end()) {
      // Step 2. ... Subsequent certificates MUST NOT have an ocsp value. [spec
      // text]
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "ocsp value found in a subsequent cert map, at index %zu.", i));
      return nullptr;
    }

    // Step 3. Each certificate’s sct value if any MUST be a
    // SignedCertificateTimestampList for that certificate as defined by Section
    // 3.3 of [RFC6962]. [spec text]
    //
    // We use SCTs only of the main certificate.
    if (i == 1) {
      auto sct_iter = cert_map.find(cbor::Value(kSctKey));
      if (sct_iter != cert_map.end()) {
        if (!sct_iter->second.is_bytestring()) {
          signed_exchange_utils::ReportErrorAndTraceEvent(
              devtools_proxy, "sct is not a bytestring.");
          return nullptr;
        }
        sct = sct_iter->second.GetBytestringAsString().as_string();
        if (sct.empty()) {
          signed_exchange_utils::ReportErrorAndTraceEvent(
              devtools_proxy, "sct must not be empty.");
          return nullptr;
        }
      }
    }
  }
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(der_certs);
  if (!cert) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "X509Certificate::CreateFromDERCertChain failed.");
    return nullptr;
  }

  return base::WrapUnique(new SignedExchangeCertificateChain(cert, ocsp, sct));
}

}  // namespace

// static
std::unique_ptr<SignedExchangeCertificateChain>
SignedExchangeCertificateChain::Parse(
    SignedExchangeVersion version,
    base::span<const uint8_t> cert_response_body,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  DCHECK_EQ(version, SignedExchangeVersion::kB2);
  return ParseB2(cert_response_body, devtools_proxy);
}

SignedExchangeCertificateChain::SignedExchangeCertificateChain(
    scoped_refptr<net::X509Certificate> cert,
    const std::string& ocsp,
    const std::string& sct)
    : cert_(cert), ocsp_(ocsp), sct_(sct) {
  DCHECK(cert);
}

SignedExchangeCertificateChain::~SignedExchangeCertificateChain() = default;

}  // namespace content
