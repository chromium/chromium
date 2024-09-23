// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "ios/public/provider/chrome/browser/signin/signin_sso_api.h"

// Configuration object used by the TrustedVaultClientBackend.
@interface TrustedVaultConfiguration : NSObject

// SingleSignOnService used by TrustedVaultClientBackend.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TRUSTED_VAULT_CONFIGURATION_H_
