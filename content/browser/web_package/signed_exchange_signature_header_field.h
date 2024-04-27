// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_HEADER_FIELD_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_HEADER_FIELD_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_export.h"
#include "net/base/hash_value.h"
#include "url/gurl.h"

namespace content {

class SignedExchangeDevToolsProxy;

// SignedExchangeSignatureHeaderField provides a parser for signed exchange's
// `Signature` header field.
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html
class CONTENT_EXPORT SignedExchangeSignatureHeaderField {
 public:
  struct CONTENT_EXPORT Signature {
    Signature();
    Signature(const Signature&);
    ~Signature();

    std::string label;
    std::string sig;
    std::string integrity;
    GURL cert_url;
    std::optional<net::SHA256HashValue> cert_sha256;
    // TODO(crbug.com/40565993): Support ed25519key.
    // std::string ed25519_key;
    signed_exchange_utils::URLWithRawString validity_url;
    uint64_t date;
    uint64_t expires;
  };

  // Parses a value of the Signature header.
  // https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#signature-header
  static std::optional<std::vector<Signature>> ParseSignature(
      std::string_view signature_str,
      SignedExchangeDevToolsProxy* devtools_proxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_SIGNATURE_HEADER_FIELD_H_
