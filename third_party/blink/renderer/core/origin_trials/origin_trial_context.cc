// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"

#include <ostream>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

void RecordTokenValidationResultHistogram(OriginTrialTokenStatus status) {
  UMA_HISTOGRAM_ENUMERATION("OriginTrials.ValidationResult", status);
}

bool IsWhitespace(UChar chr) {
  return (chr == ' ') || (chr == '\t');
}

bool SkipWhiteSpace(const String& str, unsigned& pos) {
  unsigned len = str.length();
  while (pos < len && IsWhitespace(str[pos]))
    ++pos;
  return pos < len;
}

// Extracts a quoted or unquoted token from an HTTP header. If the token was a
// quoted string, this also removes the quotes and unescapes any escaped
// characters. Also skips all whitespace before and after the token.
String ExtractTokenOrQuotedString(const String& header_value, unsigned& pos) {
  unsigned len = header_value.length();
  String result;
  if (!SkipWhiteSpace(header_value, pos))
    return String();

  if (header_value[pos] == '\'' || header_value[pos] == '"') {
    StringBuilder out;
    // Quoted string, append characters until matching quote is found,
    // unescaping as we go.
    UChar quote = header_value[pos++];
    while (pos < len && header_value[pos] != quote) {
      if (header_value[pos] == '\\')
        pos++;
      if (pos < len)
        out.Append(header_value[pos++]);
    }
    if (pos < len)
      pos++;
    result = out.ToString();
  } else {
    // Unquoted token. Consume all characters until whitespace or comma.
    int start_pos = pos;
    while (pos < len && !IsWhitespace(header_value[pos]) &&
           header_value[pos] != ',')
      pos++;
    result = header_value.Substring(start_pos, pos - start_pos);
  }
  SkipWhiteSpace(header_value, pos);
  return result;
}

// Returns whether the given feature can be activated across navigations. Only
// features reviewed and approved by security reviewers can be activated across
// navigations.
bool IsCrossNavigationFeature(OriginTrialFeature feature) {
  return origin_trials::GetNavigationOriginTrialFeatures().Contains(feature);
}

std::ostream& operator<<(std::ostream& stream, OriginTrialTokenStatus status) {
// Included for debug builds only for reduced binary size.
#ifndef NDEBUG
  switch (status) {
    case OriginTrialTokenStatus::kSuccess:
      return stream << "kSuccess";
    case OriginTrialTokenStatus::kNotSupported:
      return stream << "kNotSupported";
    case OriginTrialTokenStatus::kInsecure:
      return stream << "kInsecure";
    case OriginTrialTokenStatus::kExpired:
      return stream << "kExpired";
    case OriginTrialTokenStatus::kWrongOrigin:
      return stream << "kWrongOrigin";
    case OriginTrialTokenStatus::kInvalidSignature:
      return stream << "kInvalidSignature";
    case OriginTrialTokenStatus::kMalformed:
      return stream << "kMalformed";
    case OriginTrialTokenStatus::kWrongVersion:
      return stream << "kWrongVersion";
    case OriginTrialTokenStatus::kFeatureDisabled:
      return stream << "kFeatureDisabled";
    case OriginTrialTokenStatus::kTokenDisabled:
      return stream << "kTokenDisabled";
    case OriginTrialTokenStatus::kFeatureDisabledForUser:
      return stream << "kFeatureDisabledForUser";
  }
  NOTREACHED();
  return stream;
#else
  return stream << (static_cast<int>(status));
#endif  // ifndef NDEBUG
}

}  // namespace

OriginTrialContext::OriginTrialContext(ExecutionContext* context)
    : trial_token_validator_(std::make_unique<TrialTokenValidator>()),
      context_(context) {}

void OriginTrialContext::SetTrialTokenValidatorForTesting(
    std::unique_ptr<TrialTokenValidator> validator) {
  trial_token_validator_ = std::move(validator);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::ParseHeaderValue(
    const String& header_value) {
  std::unique_ptr<Vector<String>> tokens(new Vector<String>);
  unsigned pos = 0;
  unsigned len = header_value.length();
  while (pos < len) {
    String token = ExtractTokenOrQuotedString(header_value, pos);
    if (!token.IsEmpty())
      tokens->push_back(token);
    // Make sure tokens are comma-separated.
    if (pos < len && header_value[pos++] != ',')
      return nullptr;
  }
  return tokens;
}

// static
void OriginTrialContext::AddTokensFromHeader(ExecutionContext* context,
                                             const String& header_value) {
  if (header_value.IsEmpty())
    return;
  std::unique_ptr<Vector<String>> tokens(ParseHeaderValue(header_value));
  if (!tokens)
    return;
  AddTokens(context, tokens.get());
}

// static
void OriginTrialContext::AddTokens(ExecutionContext* context,
                                   const Vector<String>* tokens) {
  if (!tokens || tokens->IsEmpty())
    return;
  DCHECK(context && context->GetOriginTrialContext());
  context->GetOriginTrialContext()->AddTokens(*tokens);
}

// static
void OriginTrialContext::ActivateNavigationFeaturesFromInitiator(
    ExecutionContext* context,
    const Vector<OriginTrialFeature>* features) {
  if (!features || features->IsEmpty())
    return;
  DCHECK(context && context->GetOriginTrialContext());
  context->GetOriginTrialContext()->ActivateNavigationFeaturesFromInitiator(
      *features);
}

// static
std::unique_ptr<Vector<String>> OriginTrialContext::GetTokens(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  const OriginTrialContext* context =
      execution_context->GetOriginTrialContext();
  if (!context || context->tokens_.IsEmpty())
    return nullptr;
  return std::make_unique<Vector<String>>(context->tokens_);
}

// static
std::unique_ptr<Vector<OriginTrialFeature>>
OriginTrialContext::GetEnabledNavigationFeatures(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  const OriginTrialContext* context =
      execution_context->GetOriginTrialContext();
  return context ? context->GetEnabledNavigationFeatures() : nullptr;
}

std::unique_ptr<Vector<OriginTrialFeature>>
OriginTrialContext::GetEnabledNavigationFeatures() const {
  if (enabled_features_.IsEmpty())
    return nullptr;
  std::unique_ptr<Vector<OriginTrialFeature>> result =
      std::make_unique<Vector<OriginTrialFeature>>();
  for (const OriginTrialFeature& feature : enabled_features_) {
    if (IsCrossNavigationFeature(feature)) {
      result->push_back(feature);
    }
  }
  return result->IsEmpty() ? nullptr : std::move(result);
}

void OriginTrialContext::AddToken(const String& token) {
  AddTokenInternal(token, GetSecurityOrigin(), IsSecureContext(), nullptr,
                   false);
}

void OriginTrialContext::AddTokenFromExternalScript(
    const String& token,
    const SecurityOrigin* origin) {
  bool is_script_origin_secure = false;
  if (origin &&
      RuntimeEnabledFeatures::ThirdPartyOriginTrialsEnabled(context_)) {
    DVLOG(1) << "AddTokenFromExternalScript: "
             << (origin ? origin->ToString() : "null");
    is_script_origin_secure = origin->IsPotentiallyTrustworthy();
  } else {
    origin = nullptr;
  }
  AddTokenInternal(token, GetSecurityOrigin(), IsSecureContext(), origin,
                   is_script_origin_secure);
}

void OriginTrialContext::AddTokenInternal(const String& token,
                                          const SecurityOrigin* origin,
                                          bool is_origin_secure,
                                          const SecurityOrigin* script_origin,
                                          bool is_script_origin_secure) {
  if (token.IsEmpty())
    return;
  tokens_.push_back(token);

  bool enabled = EnableTrialFromToken(origin, is_origin_secure, script_origin,
                                      is_script_origin_secure, token);
  if (enabled) {
    // Only install pending features if the provided token is valid.
    // Otherwise, there was no change to the list of enabled features.
    InitializePendingFeatures();
  }
}

void OriginTrialContext::AddTokens(const Vector<String>& tokens) {
  if (tokens.IsEmpty())
    return;
  bool found_valid = false;
  for (const String& token : tokens) {
    if (!token.IsEmpty()) {
      tokens_.push_back(token);
      if (EnableTrialFromToken(GetSecurityOrigin(), IsSecureContext(), token))
        found_valid = true;
    }
  }
  if (found_valid) {
    // Only install pending features if at least one of the provided tokens are
    // valid. Otherwise, there was no change to the list of enabled features.
    InitializePendingFeatures();
  }
}

void OriginTrialContext::ActivateNavigationFeaturesFromInitiator(
    const Vector<OriginTrialFeature>& features) {
  for (const OriginTrialFeature& feature : features) {
    if (IsCrossNavigationFeature(feature)) {
      navigation_activated_features_.insert(feature);
    }
  }
  InitializePendingFeatures();
}

void OriginTrialContext::InitializePendingFeatures() {
  if (!enabled_features_.size() && !navigation_activated_features_.size())
    return;
  auto* window = DynamicTo<LocalDOMWindow>(context_.Get());
  // Normally, LocalDOMWindow::document() doesn't need to be null-checked.
  // However, this is a rare function that can get called between when the
  // LocalDOMWindow is constructed and the Document is installed. We are not
  // ready for script in that case, so bail out.
  if (!window || !window->document())
    return;
  LocalFrame* frame = window->GetFrame();
  if (!frame)
    return;
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return;
  if (!script_state->ContextIsValid())
    return;
  ScriptState::Scope scope(script_state);
  for (OriginTrialFeature enabled_feature : enabled_features_) {
    InstallFeature(enabled_feature, script_state);
  }
  for (OriginTrialFeature enabled_feature : navigation_activated_features_) {
    InstallFeature(enabled_feature, script_state);
  }
}

void OriginTrialContext::InstallFeature(OriginTrialFeature enabled_feature,
                                        ScriptState* script_state) {
  if (installed_features_.Contains(enabled_feature))
    return;
#if defined(USE_BLINK_V8_BINDING_NEW_IDL_INTERFACE)
  InstallPropertiesPerFeature(script_state, enabled_feature);
#else
  InstallPendingOriginTrialFeature(enabled_feature, script_state);
#endif
  installed_features_.insert(enabled_feature);
}

void OriginTrialContext::AddFeature(OriginTrialFeature feature) {
  enabled_features_.insert(feature);
  InitializePendingFeatures();
}

bool OriginTrialContext::IsFeatureEnabled(OriginTrialFeature feature) const {
  return enabled_features_.Contains(feature) ||
         navigation_activated_features_.Contains(feature);
}

base::Time OriginTrialContext::GetFeatureExpiry(OriginTrialFeature feature) {
  if (!IsFeatureEnabled(feature))
    return base::Time();

  auto it = feature_expiry_times_.find(feature);
  if (it == feature_expiry_times_.end())
    return base::Time();

  return it->value;
}

bool OriginTrialContext::IsNavigationFeatureActivated(
    OriginTrialFeature feature) const {
  return navigation_activated_features_.Contains(feature);
}

void OriginTrialContext::AddForceEnabledTrials(
    const Vector<String>& trial_names) {
  bool is_valid = false;
  for (const auto& trial_name : trial_names) {
    DCHECK(origin_trials::IsTrialValid(trial_name));
    is_valid |=
        EnableTrialFromName(trial_name, /*expiry_time=*/base::Time::Max());
  }

  if (is_valid) {
    // Only install pending features if at least one trial is valid. Otherwise
    // there was no change to the list of enabled features.
    InitializePendingFeatures();
  }
}

bool OriginTrialContext::CanEnableTrialFromName(const StringView& trial_name) {
  if (trial_name == "Portals" &&
      !base::FeatureList::IsEnabled(features::kPortals)) {
    return false;
  }
  if (trial_name == "AppCache" &&
      !base::FeatureList::IsEnabled(features::kAppCache)) {
    return false;
  }
  if (trial_name == "TrustTokens" &&
      !base::FeatureList::IsEnabled(network::features::kTrustTokens)) {
    return false;
  }
  if (trial_name == "InterestCohortAPI" &&
      !base::FeatureList::IsEnabled(features::kInterestCohortAPIOriginTrial)) {
    return false;
  }
  if (trial_name == "SpeculationRulesPrefetchProxy" &&
      !base::FeatureList::IsEnabled(features::kSpeculationRulesPrefetchProxy)) {
    return false;
  }
  return true;
}

bool OriginTrialContext::EnableTrialFromName(const String& trial_name,
                                             base::Time expiry_time) {
  if (!CanEnableTrialFromName(trial_name)) {
    DVLOG(1) << "EnableTrialFromName: cannot enable trial " << trial_name;
    return false;
  }

  bool did_enable_feature = false;
  for (OriginTrialFeature feature :
       origin_trials::FeaturesForTrial(trial_name)) {
    if (!origin_trials::FeatureEnabledForOS(feature)) {
      DVLOG(1) << "EnableTrialFromName: feature " << static_cast<int>(feature)
               << " is disabled on current OS.";
      continue;
    }

    did_enable_feature = true;
    enabled_features_.insert(feature);

    // Use the latest expiry time for the feature.
    if (GetFeatureExpiry(feature) < expiry_time)
      feature_expiry_times_.Set(feature, expiry_time);

    // Also enable any features implied by this feature.
    for (OriginTrialFeature implied_feature :
         origin_trials::GetImpliedFeatures(feature)) {
      enabled_features_.insert(implied_feature);

      // Use the latest expiry time for the implied feature.
      if (GetFeatureExpiry(implied_feature) < expiry_time)
        feature_expiry_times_.Set(implied_feature, expiry_time);
    }
  }
  return did_enable_feature;
}

OriginTrialTokenStatus OriginTrialContext::ValidateTokenResult(
    const String& trial_name,
    bool is_origin_secure,
    bool is_script_origin_secure,
    bool is_third_party) {
  bool is_secure = is_origin_secure;
  if (is_third_party) {
    if (!origin_trials::IsTrialEnabledForThirdPartyOrigins(trial_name)) {
      DVLOG(1) << "ValidateTokenResult: feature disabled for third party trial";
      return OriginTrialTokenStatus::kFeatureDisabled;
    }
    // For third-party tokens, both the current origin and the the script origin
    // must be secure.
    is_secure &= is_script_origin_secure;
  }

  // Origin trials are only enabled for secure origins. The only exception
  // is for deprecation trials.
  if (!is_secure &&
      !origin_trials::IsTrialEnabledForInsecureContext(trial_name)) {
    DVLOG(1) << "ValidateTokenResult: not secure";
    return OriginTrialTokenStatus::kInsecure;
  }
  return OriginTrialTokenStatus::kSuccess;
}

bool OriginTrialContext::EnableTrialFromToken(const SecurityOrigin* origin,
                                              bool is_secure,
                                              const String& token) {
  return EnableTrialFromToken(origin, is_secure, nullptr, false, token);
}

bool OriginTrialContext::EnableTrialFromToken(
    const SecurityOrigin* origin,
    bool is_origin_secure,
    const SecurityOrigin* script_origin,
    bool is_script_origin_secure,
    const String& token) {
  DCHECK(!token.IsEmpty());
  bool valid = false;
  StringUTF8Adaptor token_string(token);
  url::Origin script_url_origin;
  if (script_origin)
    script_url_origin = script_origin->ToUrlOrigin();
  TrialTokenResult token_result = trial_token_validator_->ValidateToken(
      token_string.AsStringPiece(), origin->ToUrlOrigin(),
      script_origin ? &script_url_origin : nullptr, base::Time::Now());
  DVLOG(1) << "EnableTrialFromToken: token_result = " << token_result.status
           << ", token = " << token;
  OriginTrialTokenStatus status = token_result.status;
  if (status == OriginTrialTokenStatus::kSuccess) {
    String trial_name = String::FromUTF8(token_result.feature_name.data(),
                                         token_result.feature_name.size());
    if (origin_trials::IsTrialValid(trial_name)) {
      status = ValidateTokenResult(trial_name, is_origin_secure,
                                   is_script_origin_secure,
                                   token_result.is_third_party);
      if (status == OriginTrialTokenStatus::kSuccess)
        valid = EnableTrialFromName(trial_name, token_result.expiry_time);
    }
  }
  RecordTokenValidationResultHistogram(status);
  return valid;
}

void OriginTrialContext::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
}

const SecurityOrigin* OriginTrialContext::GetSecurityOrigin() {
  const SecurityOrigin* origin;
  CHECK(context_);
  // Determines the origin to be validated against tokens:
  //  - For the purpose of origin trials, we consider worklets as running in the
  //    same context as the originating document. Thus, the special logic here
  //    to use the origin from the document context.
  if (auto* scope = DynamicTo<WorkletGlobalScope>(context_.Get()))
    origin = scope->DocumentSecurityOrigin();
  else
    origin = context_->GetSecurityOrigin();
  return origin;
}

bool OriginTrialContext::IsSecureContext() {
  bool is_secure = false;
  CHECK(context_);
  // Determines if this is a secure context:
  //  - For worklets, they are currently spec'd to not be secure, given their
  //    scope has unique origin:
  //    https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  //  - For the purpose of origin trials, we consider worklets as running in the
  //    same context as the originating document. Thus, the special logic here
  //    to check the secure status of the document context.
  if (auto* scope = DynamicTo<WorkletGlobalScope>(context_.Get())) {
    is_secure = scope->DocumentSecureContext();
  } else {
    is_secure = context_->IsSecureContext();
  }
  return is_secure;
}

}  // namespace blink
