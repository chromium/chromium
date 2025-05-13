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

bool IsFedCmMultipleIdentityProvidersEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmMultipleIdentityProviders);
}

bool IsFedCmMetricsEndpointEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmMetricsEndpoint);
}

bool IsFedCmDelegationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmDelegation);
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

bool IsWebIdentityDigitalCredentialsCreationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kWebIdentityDigitalCredentialsCreation);
}

bool IsFedCmSameSiteLaxEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSameSiteLax);
}

bool IsFedCmShowFilteredAccountsEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmShowFilteredAccounts);
}

bool IsFedCmLightweightModeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmLightweightMode);
}

bool IsFedCmAlternativeIdentifiersEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAlternativeIdentifiers);
}

bool IsFedCmCooldownOnIgnoreEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmCooldownOnIgnore);
}

bool IsFedCmUseOtherAccountAndLabelsNewSyntaxEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmUseOtherAccountAndLabelsNewSyntax);
}

bool IsFedCmAutofillEnabled() {
  // FedCmAutofill is a new flag extracted from FedCmDelegation. To avoid
  // breaking existing developer testing, we consider the new flag being enabled
  // if the old one is enabled.
  return base::FeatureList::IsEnabled(features::kFedCmAutofill) ||
         IsFedCmDelegationEnabled();
}

bool IsFedCmIframeOriginEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIframeOrigin);
}

}  // namespace content
