// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_SIGNED_REDEMPTION_RECORD_SERIALIZATION_H_
#define SERVICES_NETWORK_TRUST_TOKENS_SIGNED_REDEMPTION_RECORD_SERIALIZATION_H_

#include <string>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"

namespace network {

// The Trust Tokens design doc [1] defines a redemption record (RR) as a
// Structured Headers Draft 15 dictionary with two "byte sequence"-typed fields,
// a body and a signature.  This method constructs such a dictionary, given the
// body and signature's contents as bytestrings.
//
// Returns nullopt on internal serialization error in the Structured Headers
// library.
//
// [1]
// https://docs.google.com/document/d/1TNnya6B8pyomDK2F1R9CL3dY10OAmqWlnCxsWyOBDVQ/edit#heading=h.7mkzvhpqb8l5
base::Optional<std::string> ConstructRedemptionRecord(
    base::span<const uint8_t> body,
    base::span<const uint8_t> signature);

// Parses a Trust Tokens Redemption Record (RR), a Structured Headers Draft 15
// dictionary, into its constituent "body" and "signature" elements, placing
// them in the output parameters.
//
// Each output argument may be nullptr, denoting a lack of interest in the
// corresponding field. (The entire record might still be parsed.)
//
// Returns true on parse success and false on parse error.
bool ParseTrustTokenRedemptionRecord(base::StringPiece record,
                                     std::string* body_out = nullptr,
                                     std::string* signature_out = nullptr);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_SIGNED_REDEMPTION_RECORD_SERIALIZATION_H_
