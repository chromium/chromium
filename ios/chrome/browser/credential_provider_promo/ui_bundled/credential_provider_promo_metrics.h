// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_constants.h"

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_METRICS_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_METRICS_H_

extern const char kIOSCredentialProviderPromoImpressionHistogram[];
extern const char kIOSCredentialProviderPromoImpressionIsReminderHistogram[];
extern const char kIOSCredentialProviderPromoOnAutofillUsedHistogram[];
extern const char
    kIOSCredentialProviderPromoOnAutofillUsedIsReminderHistogram[];
extern const char kIOSCredentialProviderPromoOnSetUpListIsReminderHistogram[];
extern const char kIOSCredentialProviderPromoOnSetUpListHistogram[];

namespace credential_provider_promo {

// Enum for histograms like
// IOS.CredentialProviderExtension.Promo.OnPasswordSaved. Entries should not be
// renumbered and numeric values should never be reused.
enum class IOSCredentialProviderPromoAction : int {
  kLearnMore = 0,
  kGoToSettings = 1,
  kRemindMeLater = 2,
  kNo = 3,
  kMaxValue = kNo,
};

// Record impression metric when a credentail provider promo is displayed.
void RecordImpression(IOSCredentialProviderPromoSource source,
                      bool is_reminder);

// Record action metric for all the user actions on the displayed promo.
void RecordAction(IOSCredentialProviderPromoSource source,
                  bool is_reminder,
                  IOSCredentialProviderPromoAction action);

}  // namespace credential_provider_promo

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_METRICS_H_
