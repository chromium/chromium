// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_PARAMETERIZATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_PARAMETERIZATION_H_

namespace network {

// Note: This file contains Trust Tokens constants that are part of the network
// service public API. Network service-internal Trust Tokens-related constants
// live in a corresponding file in the network service-internal Trust Tokens
// directory.

// The maximum number of trust token issuers allowed to be associated with a
// given top-level origin.
//
// This value is quite low because registering additional issuers with an origin
// has a number of privacy risks (for instance, whether or not a user has any
// tokens issued by a given issuer reveals one bit of identifying information).
constexpr int kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers = 2;

// When the client provides custom signing data alongside a Trust Tokens signed
// request, this is the data's maximum length in bytes.
constexpr size_t kTrustTokenAdditionalSigningDataMaxSizeBytes = 1 << 11;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRUST_TOKEN_PARAMETERIZATION_H_
