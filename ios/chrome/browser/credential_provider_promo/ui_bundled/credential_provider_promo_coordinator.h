// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"

@protocol PromosManagerUIHandler;

@interface CredentialProviderPromoCoordinator
    : ChromeCoordinator <CredentialProviderPromoCommands>

// The promos manager ui handler to alert about UI changes.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
