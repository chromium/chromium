// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void InstallOriginTrialFeaturesDefault(
    const WrapperTypeInfo* wrapper_type_info,
    const ScriptState* script_state,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object) {}

void InstallPendingOriginTrialFeatureDefault(OriginTrialFeature feature,
                                             const ScriptState* script_state) {}

namespace {
InstallOriginTrialFeaturesFunction g_install_origin_trial_features_function =
    &InstallOriginTrialFeaturesDefault;

InstallPendingOriginTrialFeatureFunction
    g_install_pending_origin_trial_feature_function =
        &InstallPendingOriginTrialFeatureDefault;
}  // namespace

InstallOriginTrialFeaturesFunction SetInstallOriginTrialFeaturesFunction(
    InstallOriginTrialFeaturesFunction
        new_install_origin_trial_features_function) {
  InstallOriginTrialFeaturesFunction original_function =
      g_install_origin_trial_features_function;
  g_install_origin_trial_features_function =
      new_install_origin_trial_features_function;
  return original_function;
}

InstallPendingOriginTrialFeatureFunction
SetInstallPendingOriginTrialFeatureFunction(
    InstallPendingOriginTrialFeatureFunction
        new_install_pending_origin_trial_feature_function) {
  InstallPendingOriginTrialFeatureFunction original_function =
      g_install_pending_origin_trial_feature_function;
  g_install_pending_origin_trial_feature_function =
      new_install_pending_origin_trial_feature_function;
  return original_function;
}

void InstallOriginTrialFeatures(const WrapperTypeInfo* type,
                                const ScriptState* script_state,
                                v8::Local<v8::Object> prototype_object,
                                v8::Local<v8::Function> interface_object) {
  (*g_install_origin_trial_features_function)(
      type, script_state, prototype_object, interface_object);
}

void InstallPendingOriginTrialFeature(OriginTrialFeature feature,
                                      const ScriptState* script_state) {
  DCHECK(script_state);
  DCHECK(script_state->GetContext() ==
         script_state->GetIsolate()->GetCurrentContext());
  DCHECK(script_state->PerContextData());
  DCHECK(script_state->World().IsMainWorld());

  (*g_install_pending_origin_trial_feature_function)(feature, script_state);
}

}  // namespace blink
