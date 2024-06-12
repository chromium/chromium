// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_

// Persisted in local state and used as Enum for
// IOS.CredentialProviderExtension.Promo.Impression histogram. Entries should
// not be renumbered and numeric values should never be reused. Entries 1 and 2
// have been depcrated.
// LINT.IfChange
enum class IOSCredentialProviderPromoSource {
  kUnknown = 0,
  kAutofillUsed = 3,
  kSetUpList = 4,
  kMaxValue = kSetUpList,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml)

enum CredentialProviderPromoContext {
  kFirstStep,
  kLearnMore,
};

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSTANTS_H_
