// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/origin_trial_features.h"

#include "base/check.h"

namespace blink {

namespace {

InstallPropertiesPerFeatureFuncType g_install_properties_per_feature_func;

}  // namespace

void InstallPropertiesPerFeature(ScriptState* script_state,
                                 mojom::blink::OriginTrialFeature feature) {
  return g_install_properties_per_feature_func(script_state, feature);
}

InstallPropertiesPerFeatureFuncType SetInstallPropertiesPerFeatureFunc(
    InstallPropertiesPerFeatureFuncType func) {
  DCHECK(func);

  InstallPropertiesPerFeatureFuncType old_func =
      g_install_properties_per_feature_func;
  g_install_properties_per_feature_func = func;
  return old_func;
}

}  // namespace blink
