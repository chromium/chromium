// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"

const char kIOSCredentialProviderPromoImpressionHistogram[] =
    "IOS.CredentialProviderExtension.Promo.Impression";
const char kIOSCredentialProviderPromoImpressionIsReminderHistogram[] =
    "IOS.CredentialProviderExtension.Promo.Impression.IsReminder";
const char kIOSCredentialProviderPromoOnAutofillUsedHistogram[] =
    "IOS.CredentialProviderExtension.Promo."
    "OnSuccessfulLoginWithAutofilledPassword";
const char kIOSCredentialProviderPromoOnAutofillUsedIsReminderHistogram[] =
    "IOS.CredentialProviderExtension.Promo."
    "OnSuccessfulLoginWithAutofilledPassword.IsReminder";
const char kIOSCredentialProviderPromoOnSetUpListIsReminderHistogram[] =
    "IOS.CredentialProviderExtension.Promo.OnSetUpList.IsReminder";
const char kIOSCredentialProviderPromoOnSetUpListHistogram[] =
    "IOS.CredentialProviderExtension.Promo.OnSetUpList";

namespace credential_provider_promo {

void RecordImpression(IOSCredentialProviderPromoSource source,
                      bool is_reminder) {
  if (is_reminder) {
    base::UmaHistogramEnumeration(
        kIOSCredentialProviderPromoImpressionIsReminderHistogram, source);
  } else {
    base::UmaHistogramEnumeration(
        kIOSCredentialProviderPromoImpressionHistogram, source);
  }
}

void RecordAction(IOSCredentialProviderPromoSource source,
                  bool is_reminder,
                  IOSCredentialProviderPromoAction action) {
  std::string name;
  switch (source) {
    case IOSCredentialProviderPromoSource::kAutofillUsed:
      name = is_reminder
                 ? kIOSCredentialProviderPromoOnAutofillUsedIsReminderHistogram
                 : kIOSCredentialProviderPromoOnAutofillUsedHistogram;
      break;
    case IOSCredentialProviderPromoSource::kSetUpList:
      name = is_reminder
                 ? kIOSCredentialProviderPromoOnSetUpListIsReminderHistogram
                 : kIOSCredentialProviderPromoOnSetUpListHistogram;
      break;
    case IOSCredentialProviderPromoSource::kUnknown:
      NOTREACHED_IN_MIGRATION();
      name = is_reminder
                 ? "IOS.CredentialProviderExtension.Promo.Unknown.IsReminder"
                 : "IOS.CredentialProviderExtension.Promo.Unknown";
      break;
  }

  base::UmaHistogramEnumeration(name.data(), action);
}

}  // namespace credential_provider_promo
