// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXPORTER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXPORTER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <vector>

#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"

// Handles exporting credentials using the Credential Exchange format
// (https://fidoalliance.org/specifications-credential-exchange-specifications).
@interface CredentialExporter : NSObject

// `window` is a presentation anchor that will be used by the OS views.
- (instancetype)initWithWindow:(UIWindow*)window NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts the export flow with the provided credentials. Converts all
// credentials to the required export format and invokes the export manager.
- (void)startExportWithPasswords:
            (std::vector<password_manager::CredentialUIEntry>)passwords
                        passkeys:
                            (std::vector<sync_pb::WebauthnCredentialSpecifics>)
                                passkeys
           securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(26.0));

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXPORTER_H_
