// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/protected_audience.h"

#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

ProtectedAudience::ProtectedAudience() = default;

ScriptValue ProtectedAudience::queryFeatureSupport(ScriptState* script_state,
                                                   const String& feature_name) {
  if (feature_name == "adComponentsLimit") {
    return ScriptValue(script_state->GetIsolate(),
                       ToV8Traits<IDLUnsignedLongLong>::ToV8(
                           script_state, MaxAdAuctionAdComponents()));
  }

  if (feature_name == "deprecatedRenderURLReplacements") {
    bool enabled =
        RuntimeEnabledFeatures::FledgeDeprecatedRenderURLReplacementsEnabled(
            ExecutionContext::From(script_state));
    return ScriptValue(script_state->GetIsolate(),
                       ToV8Traits<IDLBoolean>::ToV8(script_state, enabled));
  }

  return ScriptValue();
}

}  // namespace blink
