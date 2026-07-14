// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/protected_audience.h"

#include <utility>
#include <variant>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
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
  if (const bool* bool_val = std::get_if<bool>(&val)) {
    return ToV8Traits<IDLBoolean>::ToV8(script_state, *bool_val);
  } else if (const size_t* size_t_val = std::get_if<size_t>(&val)) {
    return ToV8Traits<IDLUnsignedLongLong>::ToV8(script_state, *size_t_val);
  } else {
    const double* double_val = std::get_if<double>(&val);
    CHECK(double_val);
    return ToV8Traits<IDLDouble>::ToV8(script_state, *double_val);
  }
}

Vector<std::pair<String, FeatureVal>> MakeFeatureStatusVector(
    ExecutionContext* execution_context) {
  Vector<std::pair<String, FeatureVal>> feature_status;
  // Hardcode default values now that Protected Audience is deprecated.
  feature_status.emplace_back(String("adComponentsLimit"),
                              FeatureVal(static_cast<size_t>(20)));
  feature_status.emplace_back(String("deprecatedRenderURLReplacements"),
                              FeatureVal(false));
  feature_status.emplace_back(String("reportingTimeout"), FeatureVal(false));
  feature_status.emplace_back(String("permitCrossOriginTrustedSignals"),
                              FeatureVal(false));
  feature_status.emplace_back(String("realTimeReporting"), FeatureVal(false));
  feature_status.emplace_back(String("selectableReportingIds"),
                              FeatureVal(false));
  feature_status.emplace_back(String("sellerNonce"), FeatureVal(false));
  feature_status.emplace_back(String("trustedSignalsKVv2"), FeatureVal(false));
  feature_status.emplace_back(String("maxGroupLifetimeMs"),
                              FeatureVal(base::Days(30).InMillisecondsF()));
  return feature_status;
}

}  // namespace

ProtectedAudience::ProtectedAudience(ExecutionContext* execution_context)
    : feature_status_(MakeFeatureStatusVector(execution_context)) {}

ScriptValue ProtectedAudience::queryFeatureSupport(ScriptState* script_state,
                                                   const String& feature_name) {
  if (feature_name == "*") {
    // Return all registered features if asked for '*'
    V8ObjectBuilder features_obj(script_state);
    for (const auto& kv : feature_status_) {
      features_obj.AddV8Value(kv.first, MakeV8Val(script_state, kv.second));
    }
    return features_obj.ToScriptObject();
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
