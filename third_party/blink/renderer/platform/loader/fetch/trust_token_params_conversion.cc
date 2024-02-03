// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/trust_token_params_conversion.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/mojom/trust_tokens.mojom-blink.h"

namespace blink {

network::OptionalTrustTokenParams ConvertTrustTokenParams(
    const std::optional<network::mojom::blink::TrustTokenParams>& maybe_in) {
  if (!maybe_in)
    return std::nullopt;
  const network::mojom::blink::TrustTokenParams& in = *maybe_in;

  network::mojom::TrustTokenParamsPtr out =
      network::mojom::TrustTokenParams::New();
  out->operation = in.operation;
  out->refresh_policy = in.refresh_policy;
  out->sign_request_data = in.sign_request_data;
  out->include_timestamp_header = in.include_timestamp_header;
  for (const scoped_refptr<const SecurityOrigin>& issuer : in.issuers) {
    out->issuers.push_back(issuer->ToUrlOrigin());
  }
  for (const String& additional_header : in.additional_signed_headers) {
    out->additional_signed_headers.push_back(additional_header.Latin1());
  }
  if (!in.possibly_unsafe_additional_signing_data.IsNull()) {
    out->possibly_unsafe_additional_signing_data =
        in.possibly_unsafe_additional_signing_data.Utf8();
  }

  return network::OptionalTrustTokenParams(std::move(out));
}

}  // namespace blink
