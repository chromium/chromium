// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_

#include "base/component_export.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"

namespace network {

// Note: Some of the constants in this file might spiritually be part of the
// network service public API and belong in the corresponding
// services/network/public file; just because they're currently here, not there,
// doesn't mean there's necessarily a reason they can't be moved.

// Priority for running blocking Trust Tokens database IO. This is given value
// USER_VISIBLE because Trust Tokens DB operations can sometimes be in the
// loading critical path, but generally only for subresources.
constexpr base::TaskPriority kTrustTokenDatabaseTaskPriority =
    base::TaskPriority::USER_VISIBLE;

// The maximum time Trust Tokens backing database writes will be buffered before
// being committed to disk. Two seconds was chosen fairly arbitrarily as a value
// close to what the cookie store uses.
constexpr base::TimeDelta kTrustTokenWriteBufferingWindow = base::Seconds(2);

// This is the path relative to the issuer origin where this
// implementation of the Trust Tokens protocol expects key
// commitments to be stored.
//
// This location will eventually be standardized; for now, it is a
// preliminary value defined in the Trust Tokens design doc.
//
// WARNING: The initial '/' is necessary so that the path is "absolute": i.e.,
// GURL::Resolve will resolve it relative to the issuer origin.
constexpr char kTrustTokenKeyCommitmentWellKnownPath[] =
    "/.well-known/trust-token-keys";

// This is the maximum size of key commitment registry that the implementation
// is willing to download during key commitment checks.
//
// A value of 4 MiB should be ample for initial experimentation and can be
// revisited if necessary.
constexpr size_t kTrustTokenKeyCommitmentRegistryMaxSizeBytes = 1 << 22;

// The maximum number of (signed, unblinded) trust tokens allowed to be stored
// concurrently, scoped per token issuer.
//
// 500 is chosen as a high-but-not-excessive value for initial experimentation.
constexpr int kTrustTokenPerIssuerTokenCapacity = 500;

// The maximum Trust Tokens batch size (i.e., number of tokens to request from
// an issuer).
constexpr int kMaximumTrustTokenIssuanceBatchSize = 100;

// When to expire a signed redemption record, assuming that the issuer declined
// to specify the optional expiry timestamp. This value was chosen in absence of
// a specific reason to pick anything shorter; it could be revisited.
constexpr base::Time kTrustTokenDefaultRedemptionRecordExpiry =
    base::Time::Max();

// Returns the maximum number of keys supported by a protocol version.
size_t TrustTokenMaxKeysForVersion(mojom::TrustTokenProtocolVersion version);

// This is a representation of the current "major" version, a notion which is
// not totally well-defined but roughly corresponds to each substantial
// collection of backwards-incompatible functional changes. We send it along
// with signed requests in the Sec-Private-State-Token-Crypto-Version header,
// because the "minor" version (the specifics of the underlying issue and
// redemption crypto) does not affect signed request processing. As of writing
// in June 2021, it's not for sure that the "major" version will stay around as
// a concept for the long haul.
constexpr char kTrustTokensMajorVersion[] = "PrivateStateTokenV3";

// Time limit in redemption frequency in seconds. This is to prevent a
// malicious site exhausting user tokens. The third consecutive token consuming
// redemption operation will fail if triggered in less than this amount of
// seconds.
constexpr int kTrustTokenPerIssuerToplevelRedemptionFrequencyLimitInSeconds =
    48 * 60 * 60;

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_PARAMETERIZATION_H_
