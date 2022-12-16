// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/ui/commands/credential_provider_promo_commands.h"

@interface CredentialProviderPromoCoordinator
    : ChromeCoordinator <CredentialProviderPromoCommands>

@end

#endif  // IOS_CHROME_BROWSER_UI_CREDENTIAL_PROVIDER_PROMO_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
