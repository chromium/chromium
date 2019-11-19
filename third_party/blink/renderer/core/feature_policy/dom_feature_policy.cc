// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/dom_feature_policy.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

bool DOMFeaturePolicy::allowsFeature(ScriptState* script_state,
                                     const String& feature) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  if (GetAvailableFeatures(execution_context).Contains(feature)) {
    auto feature_name = GetDefaultFeatureNameMap().at(feature);
    mojom::PolicyValueType feature_type =
        GetPolicy()->GetFeatureList().at(feature_name).second;
    PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
    return GetPolicy()->IsFeatureEnabled(feature_name, value);
  }

  AddWarningForUnrecognizedFeature(feature);
  return false;
}

bool DOMFeaturePolicy::allowsFeature(ScriptState* script_state,
                                     const String& feature,
                                     const String& url) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(url);
  if (!origin || origin->IsOpaque()) {
    GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        "Invalid origin url for feature '" + feature + "': " + url + "."));
    return false;
  }

  if (!GetAvailableFeatures(execution_context).Contains(feature)) {
    AddWarningForUnrecognizedFeature(feature);
    return false;
  }

  auto feature_name = GetDefaultFeatureNameMap().at(feature);
  mojom::PolicyValueType feature_type =
      GetPolicy()->GetFeatureList().at(feature_name).second;
  PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
  return GetPolicy()->IsFeatureEnabledForOrigin(feature_name,
                                                origin->ToUrlOrigin(), value);
}

Vector<String> DOMFeaturePolicy::features(ScriptState* script_state) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  return GetAvailableFeatures(execution_context);
}

Vector<String> DOMFeaturePolicy::allowedFeatures(
    ScriptState* script_state) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  Vector<String> allowed_features;
  for (const String& feature : GetAvailableFeatures(execution_context)) {
    auto feature_name = GetDefaultFeatureNameMap().at(feature);
    mojom::PolicyValueType feature_type =
        GetPolicy()->GetFeatureList().at(feature_name).second;
    PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
    if (GetPolicy()->IsFeatureEnabled(feature_name, value))
      allowed_features.push_back(feature);
  }
  return allowed_features;
}

Vector<String> DOMFeaturePolicy::getAllowlistForFeature(
    ScriptState* script_state,
    const String& feature) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  if (GetAvailableFeatures(execution_context).Contains(feature)) {
    auto feature_name = GetDefaultFeatureNameMap().at(feature);
    auto feature_type = GetPolicy()->GetFeatureList().at(feature_name).second;

    const FeaturePolicy::Allowlist allowlist =
        GetPolicy()->GetAllowlistForFeature(feature_name);
    auto values = allowlist.Values();
    PolicyValue max_value = PolicyValue::CreateMaxPolicyValue(feature_type);
    if (values.empty()) {
      if (allowlist.GetFallbackValue().Type() !=
              mojom::PolicyValueType::kNull &&
          allowlist.GetFallbackValue() >= max_value)
        return Vector<String>({"*"});
    }
    Vector<String> result;
    for (const auto& entry : values) {
      result.push_back(WTF::String::FromUTF8(entry.first.Serialize()));
    }
    return result;
  }

  AddWarningForUnrecognizedFeature(feature);
  return Vector<String>();
}

void DOMFeaturePolicy::AddWarningForUnrecognizedFeature(
    const String& feature) const {
  GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      "Unrecognized feature: '" + feature + "'."));
}

void DOMFeaturePolicy::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

void DOMFeaturePolicy::UpdateContainerPolicy(
    const ParsedFeaturePolicy& container_policy,
    scoped_refptr<const SecurityOrigin> src_origin) {}

}  // namespace blink
