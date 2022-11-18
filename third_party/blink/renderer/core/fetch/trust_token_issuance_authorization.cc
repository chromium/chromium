// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/trust_token_issuance_authorization.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool IsTrustTokenIssuanceAvailableInExecutionContext(
    const ExecutionContext& context) {
  return context.FeatureEnabled(OriginTrialFeature::kPrivateStateTokens) ||
         RuntimeEnabledFeatures::PrivateStateTokensAlwaysAllowIssuanceEnabled();
}

}  // namespace blink
