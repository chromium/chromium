// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"

@class CredentialProviderPromoCoordinator;
@protocol PromosManagerUIHandler;

// Protocol for delegating the task of opening some iOS settings page.
@protocol CredentialProviderPromoCoordinatorSettingsOpenerDelegate

// Called when the user tapped the button that opens the iOS credential provider
// settings.
- (void)credentialProviderPromoCoordinatorOpenIOSCredentialProviderSettings:
    (CredentialProviderPromoCoordinator*)credentialProviderPromoCoordinator;

@end

@interface CredentialProviderPromoCoordinator
    : ChromeCoordinator <CredentialProviderPromoCommands>

// The promos manager ui handler to alert about UI changes.
@property(nonatomic, weak) id<PromosManagerUIHandler> promosUIHandler;

// The delegate that handles opening the relevant iOS settings page. Can be
// nil, in which case the coordinator will take care of opening the right
// settings page.
@property(nonatomic, weak)
    id<CredentialProviderPromoCoordinatorSettingsOpenerDelegate>
        settingsOpenerDelegate;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_COORDINATOR_H_
