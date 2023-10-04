// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class ScriptState;

using InstallPropertiesPerFeatureFuncType = void (*)(ScriptState*,
                                                     OriginTrialFeature);

// Install ES properties associated with the given origin trial feature.
PLATFORM_EXPORT void InstallPropertiesPerFeature(ScriptState* script_state,
                                                 OriginTrialFeature feature);

PLATFORM_EXPORT InstallPropertiesPerFeatureFuncType
SetInstallPropertiesPerFeatureFunc(InstallPropertiesPerFeatureFuncType func);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_
