// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/trust_token_http_headers.h"

#include "base/no_destructor.h"
#include "base/strings/string_piece.h"

namespace network {

const std::vector<base::StringPiece>& TrustTokensRequestHeaders() {
  static base::NoDestructor<std::vector<base::StringPiece>> headers{
      {kTrustTokensRequestHeaderSecSignature,
       kTrustTokensRequestHeaderSecRedemptionRecord,
       kTrustTokensRequestHeaderSecTime, kTrustTokensSecTrustTokenHeader,
       kTrustTokensSecTrustTokenVersionHeader,
       kTrustTokensRequestHeaderSecTrustTokensAdditionalSigningData,
       kTrustTokensResponseHeaderSecTrustTokenLifetime}};
  return *headers;
}

}  // namespace network
