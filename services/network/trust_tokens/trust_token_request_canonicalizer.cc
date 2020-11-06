// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_canonicalizer.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/trust_token_request_signing_helper.h"

namespace network {

base::Optional<std::vector<uint8_t>>
TrustTokenRequestCanonicalizer::Canonicalize(
    const GURL& destination,
    const net::HttpRequestHeaders& headers,
    base::StringPiece public_key,
    mojom::TrustTokenSignRequestData sign_request_data) const {
  DCHECK(sign_request_data == mojom::TrustTokenSignRequestData::kInclude ||
         sign_request_data == mojom::TrustTokenSignRequestData::kHeadersOnly);

  // It seems like there's no conceivable way in which keys could be empty
  // during normal use, so reject in this case as a common-sense safety measure.
  if (public_key.empty())
    return base::nullopt;

  cbor::Value::MapValue canonicalized_request;

  // Here and below, the lines beginning with numbers are a reproduction of the
  // normative pseudocode from the design doc.
  // 1. If sign-request-data is 'include', add 'url': <request's destination's
  // eTLD+1> to the structure.
  // 1a. The key and value are both of CBOR type “text string”.
  if (sign_request_data == mojom::TrustTokenSignRequestData::kInclude) {
    canonicalized_request.emplace(
        TrustTokenRequestSigningHelper::kCanonicalizedRequestDataDestinationKey,
        net::registry_controlled_domains::GetDomainAndRegistry(
            destination,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  // 2. If sign-request-data is 'include' or 'headers-only', for each value
  // header_name in the Signed-Headers request header, if the request has a
  // header with a name that is a case-insensitive match of header_name, add
  // <lowercased(header_name)>: <header value> to the map.
  // - Each key and value are of CBOR type “text string”.
  std::vector<std::string> headers_to_add;
  std::string signed_headers_header;
  if (headers.GetHeader(kTrustTokensRequestHeaderSignedHeaders,
                        &signed_headers_header)) {
    base::Optional<std::vector<std::string>> maybe_headers_to_add =
        internal::ParseTrustTokenSignedHeadersHeader(signed_headers_header);
    if (!maybe_headers_to_add)
      return base::nullopt;
    headers_to_add.swap(*maybe_headers_to_add);
  }

  for (const std::string& header_name : headers_to_add) {
    std::string header_value;
    if (headers.GetHeader(header_name, &header_value)) {
      canonicalized_request.emplace(base::ToLowerASCII(header_name),
                                    header_value);
    }
  }

  // 3. Add 'public-key': <pk> to the map
  // - The key is of CBOR type “text string”; the value is of CBOR type “byte
  // string”.
  canonicalized_request.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(TrustTokenRequestSigningHelper::
                                kCanonicalizedRequestDataPublicKeyKey),
      std::forward_as_tuple(public_key, cbor::Value::Type::BYTE_STRING));

  return cbor::Writer::Write(cbor::Value(std::move(canonicalized_request)));
}

}  // namespace network
