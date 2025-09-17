// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_EXCHANGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_EXCHANGE_COMMANDS_H_

// Commands related to credential exchange feature.
@protocol CredentialExchangeCommands <NSObject>

// Starts the credential exchange import flow. `UUID` is a token passed by the
// OS during app launch, required to receive the credential data.
- (void)showCredentialExchangeImport:(NSUUID*)UUID;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_CREDENTIAL_EXCHANGE_COMMANDS_H_
