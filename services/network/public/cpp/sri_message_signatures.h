// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/sri_message_signature.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

// Parses the HTTP Message Signature response headers relevant to SRI.
//
// We currently implement only the small subset of RFC9421 necessary to support
// signature-based SRI's initial milestone. Eventually, we'll have a more robust
// parser, but for the moment we limit ourselves to those headers which match
// the tight constraints described in
// https://wicg.github.io/signature-based-sri/#verification-requirements-for-sri
COMPONENT_EXPORT(NETWORK_CPP)
std::vector<mojom::SRIMessageSignaturePtr> ParseSRIMessageSignaturesFromHeaders(
    const net::HttpResponseHeaders& headers);

// Given an SRI Message Signature, and a set of response headers, construct
// the "signature base" as per Section 2.5 of RFC9421. Returns `std::nullopt`
// if no base can be constructed.
//
// https://www.rfc-editor.org/rfc/rfc9421.html#name-creating-the-signature-base
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<std::string> ConstructSignatureBase(
    const mojom::SRIMessageSignaturePtr& signature,
    const net::HttpResponseHeaders& headers);

// Validates a response's SRI-relevant HTTP Message Signatures.
//
// HTTP Message Signatures that meet the validation requirements noted above can
// be validated as soon as response headers are available. This function does
// that work, returning `true` if validation succeeds.
COMPONENT_EXPORT(NETWORK_CPP)
bool ValidateSRIMessageSignaturesOverHeaders(
    const std::vector<mojom::SRIMessageSignaturePtr>& signatures,
    const net::HttpResponseHeaders& headers);

// Returns `BlockedByResponseReason::kSRIMessageSignatureMismatch` if a response
// fails validation. If validation is successful, returns `std::nullopt`.
//
// Validation always succeeds if the `features::kSRIMessageSignatureEnforcement`
// flag is disabled.
COMPONENT_EXPORT(NETWORK_CPP)
std::optional<mojom::BlockedByResponseReason>
MaybeBlockResponseForSRIMessageSignature(
    const network::mojom::URLResponseHead& response);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_
