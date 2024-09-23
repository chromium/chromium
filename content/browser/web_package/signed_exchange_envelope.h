// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "content/browser/web_package/signed_exchange_signature_header_field.h"
#include "content/common/content_export.h"
#include "crypto/sha2.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace content {

class SignedExchangeDevToolsProxy;

// SignedExchangeEnvelope contains all information captured in
// the "application/signed-exchange" format but the payload.
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html
class CONTENT_EXPORT SignedExchangeEnvelope {
 public:
  using HeaderMap = std::map<std::string, std::string>;

  // Parse headers from the application/signed-exchange;v=b3 format.
  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#application-signed-exchange
  //
  // This also performs the steps 1, 3 and 4 of "Cross-origin trust" validation.
  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#cross-origin-trust
  static std::optional<SignedExchangeEnvelope> Parse(
      SignedExchangeVersion version,
      const signed_exchange_utils::URLWithRawString& fallback_url,
      std::string_view signature_header_field,
      base::span<const uint8_t> cbor_header,
      SignedExchangeDevToolsProxy* devtools_proxy);
  SignedExchangeEnvelope();
  SignedExchangeEnvelope(const SignedExchangeEnvelope&);
  SignedExchangeEnvelope(SignedExchangeEnvelope&&);
  SignedExchangeEnvelope& operator=(SignedExchangeEnvelope&&);
  ~SignedExchangeEnvelope();

  // AddResponseHeader returns false on duplicated keys. |name| must be
  // lower-cased.
  bool AddResponseHeader(std::string_view name, std::string_view value);
  // SetResponseHeader replaces existing value, if any. |name| must be
  // lower-cased.
  void SetResponseHeader(std::string_view name, std::string_view value);
  scoped_refptr<net::HttpResponseHeaders> BuildHttpResponseHeaders() const;

  const base::span<const uint8_t> cbor_header() const {
    return base::make_span(cbor_header_);
  }
  void set_cbor_header(base::span<const uint8_t> data);

  const signed_exchange_utils::URLWithRawString& request_url() const {
    return request_url_;
  }
  void set_request_url(const signed_exchange_utils::URLWithRawString& url) {
    request_url_ = url;
  }

  net::HttpStatusCode response_code() const { return response_code_; }
  void set_response_code(net::HttpStatusCode c) { response_code_ = c; }

  const HeaderMap& response_headers() const { return response_headers_; }

  const SignedExchangeSignatureHeaderField::Signature& signature() const {
    return signature_;
  }
  void SetSignatureForTesting(
      const SignedExchangeSignatureHeaderField::Signature& sig) {
    signature_ = sig;
  }

  // Returns the header integrity value of the loaded signed exchange.
  net::SHA256HashValue ComputeHeaderIntegrity() const;

 private:
  std::vector<uint8_t> cbor_header_;

  signed_exchange_utils::URLWithRawString request_url_;
  net::HttpStatusCode response_code_;
  HeaderMap response_headers_;
  SignedExchangeSignatureHeaderField::Signature signature_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_ENVELOPE_H_
