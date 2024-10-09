// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flags.h"

#include <optional>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

std::optional<bool> IsFedCmAuthzOverridden() {
  return base::FeatureList::GetStateIfOverridden(features::kFedCmAuthz);
}

bool IsFedCmAuthzFlagEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAuthz);
}

bool IsFedCmMultipleIdentityProvidersEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmMultipleIdentityProviders);
}

FedCmIdpSigninStatusMode GetFedCmIdpSigninStatusFlag() {
  if (base::FeatureList::IsEnabled(features::kFedCmIdpSigninStatusEnabled)) {
    return FedCmIdpSigninStatusMode::ENABLED;
  }
  return FedCmIdpSigninStatusMode::METRICS_ONLY;
}

bool IsFedCmMetricsEndpointEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmMetricsEndpoint);
}

bool IsFedCmSelectiveDisclosureEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSelectiveDisclosure);
}

bool IsFedCmSameSiteNoneEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSameSiteNone);
}

bool IsFedCmIdPRegistrationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIdPRegistration);
}

bool IsFedCmWithoutWellKnownEnforcementEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmWithoutWellKnownEnforcement);
}

bool IsWebIdentityDigitalCredentialsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebIdentityDigitalCredentials);
}

bool IsFedCmUseOtherAccountEnabled(bool is_active_mode) {
  // TODO(crbug.com/328470597): this feature is bundled with the active mode at
  // the moment. We should decouple them when supporting the feature in the
  // passive flow.
  return base::FeatureList::IsEnabled(features::kFedCmUseOtherAccount) ||
         (IsFedCmActiveModeEnabled() && is_active_mode);
}

bool IsFedCmActiveModeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmButtonMode);
}

bool IsFedCmSameSiteLaxEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSameSiteLax);
}

bool IsFedCmFlexibleFieldsEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmFlexibleFields);
}

}  // namespace content
