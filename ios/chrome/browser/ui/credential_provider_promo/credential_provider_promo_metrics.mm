// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace credential_provider_promo {

void RecordImpression(IOSCredentialProviderPromoSource source,
                      bool is_reminder) {
  if (is_reminder) {
    base::UmaHistogramEnumeration(
        "IOS.CredentialProviderExtension.Promo.Impression.IsReminder", source);
  } else {
    base::UmaHistogramEnumeration(
        "IOS.CredentialProviderExtension.Promo.Impression", source);
  }
}

void RecordAction(IOSCredentialProviderPromoSource source,
                  bool is_reminder,
                  IOSCredentialProviderPromoAction action) {
  std::string name;
  switch (source) {
    case IOSCredentialProviderPromoSource::kPasswordCopied:
      name = is_reminder
                 ? "IOS.CredentialProviderExtension.Promo.OnPasswordCopied."
                   "IsReminder"
                 : "IOS.CredentialProviderExtension.Promo.OnPasswordCopied";
      break;
    case IOSCredentialProviderPromoSource::kPasswordSaved:
      name = is_reminder
                 ? "IOS.CredentialProviderExtension.Promo.OnPasswordSaved."
                   "IsReminder"
                 : "IOS.CredentialProviderExtension.Promo.OnPasswordSaved";
      break;
    case IOSCredentialProviderPromoSource::kAutofillUsed:
      name = is_reminder ? "IOS.CredentialProviderExtension.Promo."
                           "OnSuccessfulLoginWithAutofilledPassword.IsReminder"
                         : "IOS.CredentialProviderExtension.Promo."
                           "OnSuccessfulLoginWithAutofilledPassword";
      break;
    case IOSCredentialProviderPromoSource::kUnknown:
      NOTREACHED();
      name = is_reminder
                 ? "IOS.CredentialProviderExtension.Promo.Unknown.IsReminder"
                 : "IOS.CredentialProviderExtension.Promo.Unknown";
      break;
  }

  base::UmaHistogramEnumeration(name.data(), action);
}

}  // namespace credential_provider_promo
