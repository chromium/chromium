// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/model/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"

BASE_FEATURE(kCredentialProviderExtensionPromo,
             "CredentialProviderExtensionPromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kCredentialProviderExtensionPromoOnPasswordSavedParam[] =
    "enable_promo_on_password_saved";
extern const char kCredentialProviderExtensionPromoOnPasswordCopiedParam[] =
    "enable_promo_on_password_copied";

bool IsCredentialProviderExtensionPromoEnabled() {
  return base::FeatureList::IsEnabled(kCredentialProviderExtensionPromo);
}

bool IsCredentialProviderExtensionPromoEnabledOnPasswordSaved() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kCredentialProviderExtensionPromo,
      kCredentialProviderExtensionPromoOnPasswordSavedParam, false);
}

bool IsCredentialProviderExtensionPromoEnabledOnPasswordCopied() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kCredentialProviderExtensionPromo,
      kCredentialProviderExtensionPromoOnPasswordCopiedParam, false);
}
