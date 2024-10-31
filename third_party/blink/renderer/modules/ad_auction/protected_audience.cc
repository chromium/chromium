// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/protected_audience.h"

#include <utility>

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-local-handle.h"

namespace blink {

namespace {

using FeatureVal = ProtectedAudience::FeatureVal;

v8::Local<v8::Value> MakeV8Val(ScriptState* script_state,
                               const FeatureVal& val) {
  if (const bool* bool_val = absl::get_if<bool>(&val)) {
    return ToV8Traits<IDLBoolean>::ToV8(script_state, *bool_val);
  } else {
    return ToV8Traits<IDLUnsignedLongLong>::ToV8(script_state,
                                                 absl::get<size_t>(val));
  }
}

WTF::Vector<std::pair<String, FeatureVal>> MakeFeatureStatusVector(
    ExecutionContext* execution_context) {
  WTF::Vector<std::pair<String, FeatureVal>> feature_status;
  feature_status.emplace_back(String("adComponentsLimit"),
                              FeatureVal(MaxAdAuctionAdComponents()));
  feature_status.emplace_back(
      String("deprecatedRenderURLReplacements"),
      FeatureVal(
          RuntimeEnabledFeatures::FledgeDeprecatedRenderURLReplacementsEnabled(
              execution_context)));
  feature_status.emplace_back(
      String("reportingTimeout"),
      FeatureVal(RuntimeEnabledFeatures::FledgeReportingTimeoutEnabled(
          execution_context)));
  feature_status.emplace_back(
      String("permitCrossOriginTrustedSignals"),
      FeatureVal(
          RuntimeEnabledFeatures::FledgePermitCrossOriginTrustedSignalsEnabled(
              execution_context)));
  feature_status.emplace_back(
      String("realTimeReporting"),
      FeatureVal(RuntimeEnabledFeatures::FledgeRealTimeReportingEnabled(
          execution_context)));
  feature_status.emplace_back(
      String("selectableReportingIds"),
      FeatureVal(RuntimeEnabledFeatures::FledgeAuctionDealSupportEnabled(
          execution_context)));
  return feature_status;
}

}  // namespace

ProtectedAudience::ProtectedAudience(ExecutionContext* execution_context)
    : feature_status_(MakeFeatureStatusVector(execution_context)) {}

ScriptValue ProtectedAudience::queryFeatureSupport(ScriptState* script_state,
                                                   const String& feature_name) {
  if (feature_name == "*" &&
      RuntimeEnabledFeatures::FledgeFeatureDetectAllEnabled(
          ExecutionContext::From(script_state))) {
    // Return all registered features if asked for '*'
    V8ObjectBuilder features_obj(script_state);
    for (const auto& kv : feature_status_) {
      features_obj.AddV8Value(kv.first, MakeV8Val(script_state, kv.second));
    }
    return features_obj.GetScriptValue();
  } else {
    for (const auto& kv : feature_status_) {
      if (kv.first == feature_name) {
        return ScriptValue(script_state->GetIsolate(),
                           MakeV8Val(script_state, kv.second));
      }
    }
  }

  return ScriptValue();
}

}  // namespace blink
