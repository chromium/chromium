// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/dom_feature_policy.h"

#include <algorithm>

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

// Permissions policy feature names whose queries through the JS introspection
// API (`allowsFeature()` and `getAllowlistForFeature()`) are excluded from the
// FeaturePolicyJSAPI* use counters. Chrome no longer supports these features,
// but pages may continue to query them indefinitely since we can't rely on
// sites to update their code. Excluding them keeps those leftover queries from
// inflating the counters.
void MaybeCountFeaturePolicyJSAPIUse(ExecutionContext* execution_context,
                                     const String& feature,
                                     WebFeature specific_feature) {
  static constexpr const char* kExcludedFeatures[] = {"browsing-topics",
                                                      "interest-cohort"};
  if (std::ranges::contains(kExcludedFeatures, feature)) {
    return;
  }
  UseCounter::Count(execution_context, WebFeature::kFeaturePolicyJSAPI);
  UseCounter::Count(execution_context, specific_feature);
}

}  // namespace

bool FeatureAvailable(const String& feature, ExecutionContext* ec) {
  bool is_isolated_context = ec && ec->IsIsolatedContext();
  return GetDefaultFeatureNameMap(is_isolated_context).Contains(feature) &&
         (!DisabledByOriginTrial(feature, ec)) &&
         (!DisabledByContext(feature, ec)) &&
         (!IsFeatureForMeasurementOnly(
             GetDefaultFeatureNameMap(is_isolated_context).at(feature)));
}

DOMFeaturePolicy::DOMFeaturePolicy(ExecutionContext* context)
    : context_(context) {}

bool DOMFeaturePolicy::allowsFeature(ScriptState* script_state,
                                     const String& feature) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  MaybeCountFeaturePolicyJSAPIUse(
      execution_context, feature,
      IsIFramePolicy() ? WebFeature::kFeaturePolicyJSAPIAllowsFeatureIFrame
                       : WebFeature::kFeaturePolicyJSAPIAllowsFeatureDocument);
  if (FeatureAvailable(feature, execution_context)) {
    bool is_isolated_context =
        execution_context && execution_context->IsIsolatedContext();
    auto feature_name =
        GetDefaultFeatureNameMap(is_isolated_context).at(feature);
    return GetPolicy()->IsFeatureEnabled(feature_name);
  }

  AddWarningForUnrecognizedFeature(feature);
  return false;
}

bool DOMFeaturePolicy::allowsFeature(ScriptState* script_state,
                                     const String& feature,
                                     const String& url) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  MaybeCountFeaturePolicyJSAPIUse(
      execution_context, feature,
      IsIFramePolicy()
          ? WebFeature::kFeaturePolicyJSAPIAllowsFeatureOriginIFrame
          : WebFeature::kFeaturePolicyJSAPIAllowsFeatureOriginDocument);
  scoped_refptr<const SecurityOrigin> origin =
      SecurityOrigin::CreateFromString(url);
  if (!origin || origin->IsOpaque()) {
    context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        StrCat(
            {"Invalid origin url for feature '", feature, "': ", url, "."})));
    return false;
  }

  if (!FeatureAvailable(feature, execution_context)) {
    AddWarningForUnrecognizedFeature(feature);
    return false;
  }

  bool is_isolated_context =
      execution_context && execution_context->IsIsolatedContext();
  auto feature_name = GetDefaultFeatureNameMap(is_isolated_context).at(feature);
  return GetPolicy()->IsFeatureEnabledForOrigin(feature_name,
                                                origin->ToUrlOrigin());
}

Vector<String> DOMFeaturePolicy::features(ScriptState* script_state) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  UseCounter::Count(execution_context,
                    IsIFramePolicy()
                        ? WebFeature::kFeaturePolicyJSAPIFeaturesIFrame
                        : WebFeature::kFeaturePolicyJSAPIFeaturesDocument);
  return GetAvailableFeatures(execution_context);
}

Vector<String> DOMFeaturePolicy::allowedFeatures(
    ScriptState* script_state) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  UseCounter::Count(
      execution_context,
      IsIFramePolicy()
          ? WebFeature::kFeaturePolicyJSAPIAllowedFeaturesIFrame
          : WebFeature::kFeaturePolicyJSAPIAllowedFeaturesDocument);
  Vector<String> allowed_features;
  bool is_isolated_context =
      execution_context && execution_context->IsIsolatedContext();
  for (const String& feature : GetAvailableFeatures(execution_context)) {
    auto feature_name =
        GetDefaultFeatureNameMap(is_isolated_context).at(feature);
    if (GetPolicy()->IsFeatureEnabled(feature_name))
      allowed_features.push_back(feature);
  }
  return allowed_features;
}

Vector<String> DOMFeaturePolicy::getAllowlistForFeature(
    ScriptState* script_state,
    const String& feature) const {
  ExecutionContext* execution_context =
      script_state ? ExecutionContext::From(script_state) : nullptr;
  MaybeCountFeaturePolicyJSAPIUse(
      execution_context, feature,
      IsIFramePolicy() ? WebFeature::kFeaturePolicyJSAPIGetAllowlistIFrame
                       : WebFeature::kFeaturePolicyJSAPIGetAllowlistDocument);
  bool is_isolated_context =
      execution_context && execution_context->IsIsolatedContext();
  if (FeatureAvailable(feature, execution_context)) {
    auto feature_name =
        GetDefaultFeatureNameMap(is_isolated_context).at(feature);

    const network::PermissionsPolicy::Allowlist allowlist =
        GetPolicy()->GetAllowlistForFeature(feature_name);
    const auto& allowed_origins = allowlist.AllowedOrigins();
    if (allowed_origins.empty()) {
      if (allowlist.MatchesAll())
        return Vector<String>({"*"});
    }
    Vector<String> result;
    result.reserve(
        static_cast<wtf_size_t>(allowed_origins.size()) +
        static_cast<wtf_size_t>(allowlist.SelfIfMatches().has_value()));
    if (allowlist.SelfIfMatches()) {
      result.push_back(
          String::FromUtf8(allowlist.SelfIfMatches()->Serialize()));
    }
    for (const auto& origin_with_possible_wildcards : allowed_origins) {
      result.push_back(
          String::FromUtf8(origin_with_possible_wildcards.Serialize()));
    }
    return result;
  }

  AddWarningForUnrecognizedFeature(feature);
  return Vector<String>();
}

void DOMFeaturePolicy::AddWarningForUnrecognizedFeature(
    const String& feature) const {
  context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kOther,
      mojom::blink::ConsoleMessageLevel::kWarning,
      StrCat({"Unrecognized feature: '", feature, "'."})));
}

void DOMFeaturePolicy::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(context_);
}

}  // namespace blink
