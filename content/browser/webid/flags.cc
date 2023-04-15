// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flags.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsFedCmAuthzEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAuthz);
}

bool IsFedCmAutoReauthnEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmAutoReauthn);
}

bool IsFedCmIdpSignoutEnabled() {
  return GetFieldTrialParamByFeatureAsBool(
      features::kFedCm, features::kFedCmIdpSignoutFieldTrialParamName, false);
}

bool IsFedCmMultipleIdentityProvidersEnabled() {
  return base::FeatureList::IsEnabled(
      features::kFedCmMultipleIdentityProviders);
}

FedCmIdpSigninStatusMode GetFedCmIdpSigninStatusMode() {
  if (GetFieldTrialParamByFeatureAsBool(
          features::kFedCm, features::kFedCmIdpSigninStatusFieldTrialParamName,
          false)) {
    return FedCmIdpSigninStatusMode::ENABLED;
  }
  if (GetFieldTrialParamByFeatureAsBool(
          features::kFedCm,
          features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
          true)) {
    return FedCmIdpSigninStatusMode::METRICS_ONLY;
  }
  return FedCmIdpSigninStatusMode::DISABLED;
}

bool IsFedCmMetricsEndpointEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmMetricsEndpoint);
}

bool IsFedCmRpContextEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmRpContext);
}

bool IsFedCmUserInfoEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmUserInfo);
}

bool IsFedCmSelectiveDisclosureEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmSelectiveDisclosure);
}

bool IsFedCmLoginHintEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmLoginHint);
}

bool IsFedCmIdPRegistrationEnabled() {
  return base::FeatureList::IsEnabled(features::kFedCmIdPRegistration);
}

bool IsWebIdentityMDocsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebIdentityMDocs);
}

}  // namespace content
