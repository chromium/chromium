// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation for this header file is auto-generated.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIALS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIALS_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

enum class OriginTrialType { kDefault = 0, kDeprecation, kIntervention };

// A namespace with dynamic tests for experimental features which can be
// enabled by the origin trials framework via origin trial tokens.
namespace origin_trials {

// Return true if there is a feature with the passed |trial_name|.
CORE_EXPORT bool IsTrialValid(const StringView& trial_name);

// Return true if |trial_name| can be enabled in an insecure context.
CORE_EXPORT bool IsTrialEnabledForInsecureContext(const StringView& trial_name);

// Return true if |trial_name| can be enabled from third party origins.
CORE_EXPORT bool IsTrialEnabledForThirdPartyOrigins(
    const StringView& trial_name);

// Returns the trial type of the given |feature|.
CORE_EXPORT OriginTrialType GetTrialType(OriginTrialFeature feature);

// Return origin trials features that are enabled by the passed |trial_name|.
// The trial name MUST be valid (call IsTrialValid() before calling this
// function).
CORE_EXPORT base::span<const OriginTrialFeature> FeaturesForTrial(
    const StringView& trial_name);

// Return the list of features which will also be enabled if the given
// |feature| is enabled.
base::span<const OriginTrialFeature> GetImpliedFeatures(
    OriginTrialFeature feature);

bool FeatureEnabledForOS(OriginTrialFeature feature);

const HashSet<OriginTrialFeature>& GetNavigationOriginTrialFeatures();

}  // namespace origin_trials

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ORIGIN_TRIALS_ORIGIN_TRIALS_H_
