// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_MIGRATOR_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_MIGRATOR_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "base/functional/callback.h"
#import "base/memory/scoped_refptr.h"
#import "base/types/expected.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace webauthn {
class PasskeyModel;
}  // namespace webauthn

// Values of the UMA Passkeys.IOSMigration histogram. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange(PasskeysMigrationStatus)
enum class PasskeysMigrationStatus {
  // New passkey from the CPE migrated to Chrome.
  kPasskeyCreated = 0,
  // Existing passkey used by the CPE updated in Chrome.
  kPasskeyUpdated = 1,
  // An invalid passkey was sent from the CPE.
  kInvalidPasskey = 2,
  kMaxValue = kInvalidPasskey
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:PasskeysMigrationStatus)

// Values of the UMA Passkeys.IOSMigration.SignalAction histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
// LINT.IfChange(PasskeysSignalMigrationAction)
enum class PasskeysSignalMigrationAction {
  // Existing passkey marked hidden via Signal API in the CPE.
  kPasskeyHidden = 0,
  // Existing passkey restored via Signal API in the CPE.
  kPasskeyRestored = 1,
  // Existing passkey details updated via Signal API in the CPE.
  kPasskeyDetailsUpdated = 2,
  kMaxValue = kPasskeyDetailsUpdated
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/password/enums.xml:PasskeysSignalMigrationAction)

// Error domain for credential provider migration errors.
extern NSErrorDomain const kCredentialProviderMigratorErrorDomain;

// Possible error codes in case of migration failure.
typedef enum : NSInteger {
  kCredentialProviderMigratorErrorAlreadyRunning,
} CredentialProviderMigratorErrors;

// Result of migrating credentials from the Credential Provider Extension:
// either the newly created passkeys migrated in this batch, or an error.
using CredentialMigrationResult =
    base::expected<std::vector<sync_pb::WebauthnCredentialSpecifics>, NSError*>;
using CredentialMigrationCompletion =
    base::OnceCallback<void(CredentialMigrationResult)>;

@interface CredentialProviderMigrator : NSObject
- (instancetype)
    initWithUserDefaults:(NSUserDefaults*)userDefaults
                     key:(NSString*)key
                    gaia:(NSString*)gaiaID
           passwordStore:
               (scoped_refptr<password_manager::PasswordStoreInterface>)
                   passwordStore
            passkeyStore:(webauthn::PasskeyModel*)passkeyStore;
- (instancetype)init NS_UNAVAILABLE;

// Starts migration from the temporal store to the password and passkey stores
// and invokes `completion` with `CredentialMigrationResult`.
- (void)startMigrationWithCompletion:(CredentialMigrationCompletion)completion;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_MODEL_CREDENTIAL_PROVIDER_MIGRATOR_H_
