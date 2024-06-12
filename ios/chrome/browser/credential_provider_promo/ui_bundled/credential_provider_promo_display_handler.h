// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_DISPLAY_HANDLER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_DISPLAY_HANDLER_H_

#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"

#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@protocol CredentialProviderPromoCommands;

// Handler for displaying the Credential Provider Promo.
//
// This handler is called by the Promos Manager and presents the Credential
// Provider promo to eligible users.
@interface CredentialProviderPromoDisplayHandler
    : NSObject <StandardPromoDisplayHandler>

- (instancetype)initWithHandler:(id<CredentialProviderPromoCommands>)handler;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_DISPLAY_HANDLER_H_
