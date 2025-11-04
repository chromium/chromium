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
#include "services/network/public/cpp/features.h"

namespace content::webid {

bool IsMetricsEndpointEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmMetricsEndpoint);
}

bool IsDelegationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmDelegation);
}

bool IsIdPRegistrationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIdPRegistration);
}

bool IsWithoutWellKnownEnforcementEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmWithoutWellKnownEnforcement);
}

bool IsDigitalCredentialsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebIdentityDigitalCredentials);
}

bool IsDigitalCredentialsCreationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kWebIdentityDigitalCredentialsCreation);
}

bool IsSameSiteLaxEnabled() {
  return base::FeatureList::IsEnabled(
      network::features::kSendSameSiteLaxForFedCM);
}

bool IsLightweightModeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmLightweightMode);
}

bool IsAlternativeIdentifiersEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAlternativeIdentifiers);
}

bool IsUseOtherAccountAndLabelsNewSyntaxEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmUseOtherAccountAndLabelsNewSyntax);
}

bool IsFedCmEmbedderCheckEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmEmbedderCheck);
}

bool IsAutofillEnabled() {
  // FedCmAutofill is a new flag extracted from FedCmDelegation. To avoid
  // breaking existing developer testing, we consider the new flag being enabled
  // if the old one is enabled.
  return base::FeatureList::IsEnabled(features::kFedCmAutofill) ||
         IsDelegationEnabled();
}

bool IsIframeOriginEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIframeOrigin);
}

bool IsNonceInParamsEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmNonceInParams);
}

bool IsNonStringTokenEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmNonStringToken);
}

bool IsWellKnownEndpointValidationEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmWellKnownEndpointValidation);
}

bool IsPreservePortsForTestingEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmPreservePortsForTesting);
}

bool IsErrorAttributeEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmErrorAttribute);
}

bool IsNavigationInterceptionEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmNavigationInterception);
}

}  // namespace content::webid
