// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/features.h"

#import <Foundation/Foundation.h>

#import "base/metrics/field_trial_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BASE_FEATURE(kCredentialProviderExtensionPromo,
             "CredentialProviderExtensionPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kCredentialProviderExtensionPromoOnPasswordSavedParam[] =
    "enable_promo_on_password_saved";
extern const char kCredentialProviderExtensionPromoOnPasswordCopiedParam[] =
    "enable_promo_on_password_copied";
extern const char kCredentialProviderExtensionPromoOnLoginWithAutofillParam[] =
    "enable_promo_on_login_with_autofill";

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

bool IsCredentialProviderExtensionPromoEnabledOnLoginWithAutofill() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kCredentialProviderExtensionPromo,
      kCredentialProviderExtensionPromoOnLoginWithAutofillParam, false);
}
