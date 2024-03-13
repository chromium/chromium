// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_PROVIDER_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_PROVIDER_PROMO_COMMANDS_H_

enum class CredentialProviderPromoTrigger {
  SuccessfulLoginUsingExistingPassword,  // User successfully logs in using
                                         // existing password.
  RemindMeLater,                         // User has tapped Remind Me Later in
                                         // the promo before.
  SetUpList,                             // User has clicked the Autofill item
                                         // in the Set Up List on the NTP.
};

// Commands to show app-wide promos.
@protocol CredentialProviderPromoCommands <NSObject>

// Show Credential Provider Promo
- (void)showCredentialProviderPromoWithTrigger:
    (CredentialProviderPromoTrigger)trigger;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_PROVIDER_PROMO_COMMANDS_H_
