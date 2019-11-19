// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIAL_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIAL_CONTEXT_H_

#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;

// The Origin Trials Framework provides limited access to experimental features,
// on a per-origin basis (origin trials). This class provides the implementation
// to check if the experimental feature should be enabled for the current
// context.  This class is not for direct use by feature implementers.
// Instead, the OriginTrials generated namespace provides a method for each
// trial to check if it is enabled. Experimental features must be defined in
// runtime_enabled_features.json5, which is used to generate origin_trials.h/cc.
//
// Origin trials are defined by string names, provided by the implementers. The
// framework does not maintain an enum or constant list for trial names.
// Instead, the name provided by the feature implementation is validated against
// any provided tokens.
//
// For more information, see https://github.com/jpchase/OriginTrials.
class CORE_EXPORT OriginTrialContext final
    : public GarbageCollected<OriginTrialContext> {
 public:
  OriginTrialContext();
  explicit OriginTrialContext(std::unique_ptr<TrialTokenValidator> validator);

  void BindExecutionContext(ExecutionContext*);

  // Parses an Origin-Trial header as specified in
  // https://jpchase.github.io/OriginTrials/#header into individual tokens.
  // Returns null if the header value was malformed and could not be parsed.
  // If the header does not contain any tokens, this returns an empty vector.
  static std::unique_ptr<Vector<String>> ParseHeaderValue(
      const String& header_value);

  static void AddTokensFromHeader(ExecutionContext*,
                                  const String& header_value);
  static void AddTokens(ExecutionContext*, const Vector<String>* tokens);

  // Returns the trial tokens that are active in a specific ExecutionContext.
  // Returns null if no tokens were added to the ExecutionContext.
  static std::unique_ptr<Vector<String>> GetTokens(ExecutionContext*);

  // Returns the navigation trial features that are enabled in the specified
  // ExecutionContext, that should be forwarded to (and activated in)
  // ExecutionContexts navigated to from the given ExecutionContext. Returns
  // null if no such trials were added to the ExecutionContext.
  static std::unique_ptr<Vector<OriginTrialFeature>>
  GetEnabledNavigationFeatures(ExecutionContext*);

  // Activates navigation trial features forwarded from the ExecutionContext
  // that navigated to the specified ExecutionContext. Only features for which
  // origin_trials::IsCrossNavigationFeature returns true can be activated via
  // this method. Trials activated via this method will return true from
  // IsNavigationFeatureActivated, for the specified ExecutionContext.
  static void ActivateNavigationFeaturesFromInitiator(
      ExecutionContext*,
      const Vector<OriginTrialFeature>*);

  void AddToken(const String& token);
  void AddTokens(const Vector<String>& tokens);
  void AddTokens(const SecurityOrigin* origin,
                 bool is_secure,
                 const Vector<String>& tokens);

  void ActivateNavigationFeaturesFromInitiator(
      const Vector<OriginTrialFeature>& features);

  // Forces a given origin-trial-enabled feature to be enabled in this context
  // and immediately adds required bindings to already initialized JS contexts.
  void AddFeature(OriginTrialFeature feature);

  // Returns true if the feature should be considered enabled for the current
  // execution context.
  bool IsFeatureEnabled(OriginTrialFeature feature) const;

  std::unique_ptr<Vector<OriginTrialFeature>> GetEnabledNavigationFeatures()
      const;

  // Returns true if the navigation feature is activated in the current
  // ExecutionContext. Navigation features are features that are enabled in one
  // ExecutionContext, but whose behavior is activated in ExecutionContexts that
  // are navigated to from that context. For example, if navigating from context
  // A to B, a navigation feature is enabled in A, and activated in B.
  bool IsNavigationFeatureActivated(const OriginTrialFeature feature) const;

  // Installs JavaScript bindings on the relevant objects for any features which
  // should be enabled by the current set of trial tokens. This method is called
  // every time a token is added to the document (including when tokens are
  // added via script). JavaScript-exposed members will be properly visible, for
  // existing objects in the V8 context. If the V8 context is not initialized,
  // or there are no enabled features, or all enabled features are already
  // initialized, this method returns without doing anything. That is, it is
  // safe to call this method multiple times, even if no trials are newly
  // enabled.
  void InitializePendingFeatures();

  void Trace(blink::Visitor*);

 private:
  // Validate the trial token. If valid, the trial named in the token is
  // added to the list of enabled trials. Returns true or false to indicate if
  // the token is valid.
  bool EnableTrialFromToken(const SecurityOrigin* origin,
                            bool is_secure,
                            const String& token);

  // Installs JavaScript bindings on the relevant objects for the specified
  // OriginTrialFeature.
  void InstallFeature(OriginTrialFeature, ScriptState*);

  const SecurityOrigin* GetSecurityOrigin();
  bool IsSecureContext();

  Vector<String> tokens_;
  HashSet<OriginTrialFeature> enabled_features_;
  HashSet<OriginTrialFeature> installed_features_;
  HashSet<OriginTrialFeature> navigation_activated_features_;
  std::unique_ptr<TrialTokenValidator> trial_token_validator_;
  Member<ExecutionContext> context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIAL_CONTEXT_H_
