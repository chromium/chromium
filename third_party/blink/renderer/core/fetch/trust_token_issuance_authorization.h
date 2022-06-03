// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_ISSUANCE_AUTHORIZATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_ISSUANCE_AUTHORIZATION_H_

namespace blink {

class ExecutionContext;

// Returns whether it's OK to execute Trust Tokens issuance in the given
// execution context. This depends on whether the context is participating in
// the kTrustTokens origin trial, and whether the embedder has specified an
// override of this requirement (e.g. for testing).
//
// For more information on Trust Tokens configuration, see the comment on
// network::features::kTrustTokens.
bool IsTrustTokenIssuanceAvailableInExecutionContext(
    const ExecutionContext& context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_TRUST_TOKEN_ISSUANCE_AUTHORIZATION_H_
