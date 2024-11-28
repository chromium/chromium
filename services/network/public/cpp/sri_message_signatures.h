// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/sri_message_signature.mojom.h"

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

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SRI_MESSAGE_SIGNATURES_H_
