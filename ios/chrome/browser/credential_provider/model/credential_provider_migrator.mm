// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator.h"

#import <deque>
#import <iterator>
#import <memory>
#import <optional>
#import <string>
#import <string_view>
#import <utility>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/containers/flat_set.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/no_destructor.h"
#import "base/sequence_checker.h"
#import "base/strings/string_view_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_store/password_form_converters.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "ios/chrome/browser/credential_provider/model/archivable_credential+password_form.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/passkey_model_observer_bridge.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

using password_manager::PasswordStoreInterface;

NSErrorDomain const kCredentialProviderMigratorErrorDomain =
    @"kCredentialProviderMigratorErrorDomain";

namespace {

// Execution phase of a serialized temporal-store pass.
enum class StorePassState {
  // No temporal-store pass is currently queued or saving.
  kIdle,
  // Waiting in `TemporalStoreSerializer` to read `NSUserDefaults`.
  kQueued,
  // Persisting updates in `UserDefaultsCredentialStore`.
  kSaving,
};

// Serializes `UserDefaultsCredentialStore` read-modify-write passes across
// `CredentialProviderMigrator` instances on the main sequence so concurrent
// profile migrations never overwrite each other's App Group `NSUserDefaults`
// updates.
class TemporalStoreSerializer {
 public:
  using Step = base::OnceCallback<void(base::OnceClosure)>;

  static TemporalStoreSerializer& GetInstance() {
    static base::NoDestructor<TemporalStoreSerializer> instance;
    return *instance;
  }

  void Enqueue(Step step) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    queue_.push_back(std::move(step));
    if (!running_) {
      RunNext();
    }
  }

 private:
  void RunNext() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (queue_.empty()) {
      running_ = false;
      return;
    }
    running_ = true;
    Step next = std::move(queue_.front());
    queue_.pop_front();
    std::move(next).Run(base::BindOnce(&TemporalStoreSerializer::OnStepFinished,
                                       base::Unretained(this)));
  }

  void OnStepFinished() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    RunNext();
  }

  SEQUENCE_CHECKER(sequence_checker_);
  bool running_ = false;
  std::deque<Step> queue_;
};

// Logs the passkey migration status to UMA.
void LogPasskeyMigrationStatus(PasskeysMigrationStatus status) {
  base::UmaHistogramEnumeration("Passkeys.IOSMigration", status);
}

// Logs the passkey signal action to UMA.
void LogPasskeySignalMigrationAction(PasskeysSignalMigrationAction action) {
  base::UmaHistogramEnumeration("Passkeys.IOSMigration.SignalAction", action);
}

// Returns a `std::string_view` over the bytes owned by `data`, or an empty view
// if `data` is nil or empty.
std::string_view StringViewFromData(NSData* data) {
  return base::as_string_view(base::apple::NSDataToSpan(data));
}

// Applies pending state updates from `credential` to `passkey_store` if the
// matching passkey is still active (not shadowed). Returns false if no active
// passkey matches `credential`.
bool UpdateExistingPasskey(webauthn::PasskeyModel& passkey_store,
                           id<Credential> credential) {
  CHECK(passkey_store.IsReady());
  std::string rp_id = base::SysNSStringToUTF8(credential.rpId);
  std::string credential_id(StringViewFromData(credential.credentialId));
  std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_store.GetPasskey(
          rp_id, credential_id,
          webauthn::PasskeyModel::ShadowedCredentials::kExclude);
  if (!credential_specifics.has_value()) {
    return false;
  }

  if (credential_specifics->hidden() != credential.hidden) {
    // TODO(crbug.com/564349705): Add PasskeyChangeQuotaTracker.
    if (credential.hidden) {
      passkey_store.HidePasskey(
          credential_id,
          base::Time::FromMillisecondsSinceUnixEpoch(credential.hiddenTime));
      LogPasskeySignalMigrationAction(
          PasskeysSignalMigrationAction::kPasskeyHidden);
    } else {
      passkey_store.UnhidePasskey(credential_id);
      LogPasskeySignalMigrationAction(
          PasskeysSignalMigrationAction::kPasskeyRestored);
    }
  }

  std::string username = base::SysNSStringToUTF8(credential.username);
  if (credential_specifics->user_name() != username) {
    passkey_store.UpdatePasskey(
        credential_id,
        {.user_name = username,
         .user_display_name = credential_specifics->user_display_name()},
        /*updated_by_user=*/false);
    LogPasskeySignalMigrationAction(
        PasskeysSignalMigrationAction::kPasskeyDetailsUpdated);
  }

  if (credential_specifics->last_used_time_windows_epoch_micros() <
      credential.lastUsedTime) {
    passkey_store.UpdatePasskeyTimestamp(
        credential_id, base::Time::FromDeltaSinceWindowsEpoch(
                           base::Microseconds(credential.lastUsedTime)));
    LogPasskeyMigrationStatus(PasskeysMigrationStatus::kPasskeyUpdated);
  }
  return true;
}

}  // namespace

@interface CredentialProviderMigrator () <PasskeyModelObserverDelegate>
@end

@implementation CredentialProviderMigrator {
  // Key used to retrieve the temporal storage.
  NSString* _key;

  // User defaults containing the temporal storage.
  NSUserDefaults* _userDefaults;

  // Password manager store, where passwords will be migrated to.
  scoped_refptr<PasswordStoreInterface> _passwordStore;

  // Passkey store, or `nullptr` if unavailable, failed initialization, or shut
  // down.
  raw_ptr<webauthn::PasskeyModel> _passkeyStore;

  // Observer for `_passkeyStore` readiness, initialization failure, and
  // shutdown events.
  std::unique_ptr<PasskeyModelObserverBridge> _passkeyModelObserverBridge;

  // Non-null from `-startMigrationWithCompletion:` until migration finishes.
  CredentialMigrationCompletion _pendingCompletion;

  // Newly created passkeys imported across passes of the current migration.
  std::vector<sync_pb::WebauthnCredentialSpecifics> _accumulatedCreatedPasskeys;

  // Current phase of the serialized temporal-store pass.
  StorePassState _passState;

  // YES while matching passkeys remain in `NSUserDefaults` awaiting
  // `_passkeyStore` readiness.
  BOOL _waitingForPasskeyModel;

  // YES if a re-trigger or readiness event arrived while `_passState` was
  // `kSaving`.
  BOOL _needsFollowUpPass;

  // YES once `_passkeyStore` shut down or a running migration was cancelled.
  // Sticky, so later `-startMigrationWithCompletion:` calls complete
  // immediately without migrating.
  BOOL _aborted;
}

- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                                 key:(NSString*)key
                                gaia:(NSString*)gaiaID
                       passwordStore:
                           (scoped_refptr<PasswordStoreInterface>)passwordStore
                        passkeyStore:(webauthn::PasskeyModel*)passkeyStore {
  self = [super init];
  if (self) {
    _key = [key copy];
    _userDefaults = userDefaults;
    _gaiaID = [gaiaID copy];
    _passwordStore = std::move(passwordStore);
    _passkeyStore = passkeyStore;
    if (_passkeyStore) {
      _passkeyModelObserverBridge =
          std::make_unique<PasskeyModelObserverBridge>(self, _passkeyStore);
    }
  }
  return self;
}

#pragma mark - Public

- (void)startMigrationWithCompletion:(CredentialMigrationCompletion)completion {
  CHECK(completion);
  if (_pendingCompletion) {
    NSError* error =
        [NSError errorWithDomain:kCredentialProviderMigratorErrorDomain
                            code:kCredentialProviderMigratorErrorAlreadyRunning
                        userInfo:nil];
    std::move(completion).Run(base::unexpected(error));
    return;
  }

  _pendingCompletion = std::move(completion);
  if (_aborted) {
    [self completeMigrationWithError:nil];
    return;
  }
  [self enqueueStorePass];
}

- (void)checkForNewCredentials {
  if (!_pendingCompletion || _aborted) {
    return;
  }
  [self enqueueStorePass];
}

- (void)cancelPendingMigration {
  if (!_pendingCompletion) {
    return;
  }

  _aborted = YES;
  _waitingForPasskeyModel = NO;
  _needsFollowUpPass = NO;
  // An in-flight temporal-store write resolves `_pendingCompletion` from
  // `-didSaveStorePassWithError:` once it finishes.
  if (_passState == StorePassState::kSaving) {
    return;
  }

  _passState = StorePassState::kIdle;
  [self completeMigrationWithError:nil];
}

#pragma mark - PasskeyModelObserverDelegate

- (void)passKeyModelShuttingDown:(webauthn::PasskeyModel*)passkeyModel {
  CHECK_EQ(_passkeyStore, passkeyModel);
  _passkeyModelObserverBridge.reset();
  _passkeyStore = nullptr;
  _passwordStore = nullptr;
  _aborted = YES;
  [self cancelPendingMigration];
}

- (void)passkeyModelIsReady:(webauthn::PasskeyModel*)passkeyModel {
  CHECK_EQ(_passkeyStore, passkeyModel);
  // `_passkeyModelObserverBridge` observes `_passkeyStore` for the migrator's
  // whole lifetime, so readiness can fire when no passkeys are awaiting import.
  if (!_waitingForPasskeyModel) {
    return;
  }

  // Importing passkeys mutates `passkeyModel`, which would re-enter the
  // observer list currently delivering `OnPasskeyModelIsReady`. Post the import
  // so it runs once the notification loop unwinds.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](CredentialProviderMigrator* migrator) {
                       [migrator migrateDeferredPasskeysIfReady];
                     },
                     weakSelf));
}

- (void)passkeyModelDidChange {
  // No-op.
}

- (void)passkeyModelDidFailInitialization:
    (webauthn::PasskeyModel*)passkeyModel {
  CHECK_EQ(_passkeyStore, passkeyModel);
  _passkeyModelObserverBridge.reset();
  _passkeyStore = nullptr;
  _waitingForPasskeyModel = NO;
  if (_passState == StorePassState::kIdle && _pendingCompletion) {
    [self completeMigrationWithError:nil];
  }
}

#pragma mark - Private

// Enqueues a serialized temporal-store read-modify-write pass, or coalesces
// re-triggers into the queued or saving pass.
- (void)enqueueStorePass {
  switch (_passState) {
    case StorePassState::kQueued:
      // The queued pass has not read `NSUserDefaults` yet, so it will pick up
      // any new credentials.
      return;
    case StorePassState::kSaving:
      // The saving pass read a now-stale snapshot; schedule another pass once
      // it finishes.
      _needsFollowUpPass = YES;
      return;
    case StorePassState::kIdle:
      break;
  }

  _passState = StorePassState::kQueued;
  __weak __typeof(self) weakSelf = self;
  TemporalStoreSerializer::GetInstance().Enqueue(base::BindOnce(
      [](CredentialProviderMigrator* migrator, base::OnceClosure done) {
        if (!migrator) {
          std::move(done).Run();
          return;
        }
        [migrator performStorePassWithDoneClosure:std::move(done)];
      },
      weakSelf));
}

// Opens a fresh `UserDefaultsCredentialStore`, migrates any matching passwords
// and (when `_passkeyStore` is ready) matching passkeys, and persists updates
// immediately so no stale snapshot is held across `PasskeyModel` waits.
- (void)performStorePassWithDoneClosure:(base::OnceClosure)done {
  // Migration may have been aborted by shutdown or backgrounding while queued
  // behind another profile's store save.
  if (!_pendingCompletion || _aborted) {
    std::move(done).Run();
    return;
  }

  UserDefaultsCredentialStore* temporalStore =
      [[UserDefaultsCredentialStore alloc] initWithUserDefaults:_userDefaults
                                                            key:_key];
  NSArray<id<Credential>>* credentials = [temporalStore.credentials copy];
  NSMutableArray<id<Credential>>* matchingPasskeys =
      [[NSMutableArray alloc] init];
  BOOL didMutateStore = NO;

  for (id<Credential> credential in credentials) {
    if ([credential isPasskey]) {
      // For passkeys, `_gaiaID` cannot be nil as passkeys require a user to be
      // signed in. Also, `_gaiaID` must match the credential's `gaia`,
      // otherwise the passkey belongs to a different account.
      if (!_gaiaID || ![credential.gaia isEqualToString:_gaiaID]) {
        continue;
      }
      [matchingPasskeys addObject:credential];
      continue;
    }

    // For passwords, either the password is a local password not associated
    // with a user account (`_gaiaID` and `credential.gaia` are both nil) or it
    // belongs to a signed-in account matching `_gaiaID`.
    BOOL validLocalPassword = !credential.gaia && !_gaiaID;
    BOOL validAccountPassword =
        _gaiaID && [credential.gaia isEqualToString:_gaiaID];
    if ((!validLocalPassword && !validAccountPassword) || !_passwordStore) {
      continue;
    }

    password_manager::PasswordForm form =
        PasswordFormFromCredential(credential);
    _passwordStore->AddLogin(
        password_manager::FromPasswordForm(std::move(form)));
    [temporalStore
        removeCredentialWithRecordIdentifier:credential.recordIdentifier];
    didMutateStore = YES;
  }

  BOOL hasDeferredPasskeys = NO;
  if (_passkeyStore && matchingPasskeys.count > 0) {
    if (_passkeyStore->IsReady()) {
      std::vector<sync_pb::WebauthnCredentialSpecifics> createdPasskeys =
          [self importPasskeyCredentials:matchingPasskeys
                               fromStore:temporalStore];
      _accumulatedCreatedPasskeys.insert(
          _accumulatedCreatedPasskeys.end(),
          std::make_move_iterator(createdPasskeys.begin()),
          std::make_move_iterator(createdPasskeys.end()));
      didMutateStore = YES;
    } else {
      hasDeferredPasskeys = YES;
    }
  }

  if (hasDeferredPasskeys) {
    _waitingForPasskeyModel = YES;
    if (!didMutateStore) {
      _passState = StorePassState::kIdle;
      std::move(done).Run();
      return;
    }
  } else {
    _waitingForPasskeyModel = NO;
    if (!didMutateStore) {
      _passState = StorePassState::kSaving;
      std::move(done).Run();
      // Finish asynchronously, matching the save path, so `_pendingCompletion`
      // never runs re-entrantly from within a store pass.
      __weak __typeof(self) weakSelf = self;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](CredentialProviderMigrator* migrator) {
                           [migrator didSaveStorePassWithError:nil];
                         },
                         weakSelf));
      return;
    }
  }

  _passState = StorePassState::kSaving;
  __weak __typeof(self) weakSelf = self;
  [temporalStore
      saveDataWithCompletion:base::CallbackToBlock(base::BindOnce(
                                 [](CredentialProviderMigrator* migrator,
                                    base::OnceClosure saveDone,
                                    NSError* error) {
                                   std::move(saveDone).Run();
                                   [migrator didSaveStorePassWithError:error];
                                 },
                                 weakSelf, std::move(done)))];
}

// Handles completion of persisting a temporal-store pass.
- (void)didSaveStorePassWithError:(NSError*)error {
  CHECK(_pendingCompletion);
  _passState = StorePassState::kIdle;

  // A cancellation that arrived while saving still reports passkeys imported
  // before it, since they already live in `_passkeyStore`.
  if (error || _aborted) {
    [self completeMigrationWithError:error];
    return;
  }

  if (_needsFollowUpPass) {
    _needsFollowUpPass = NO;
    [self enqueueStorePass];
    return;
  }

  if (_waitingForPasskeyModel) {
    return;
  }

  [self completeMigrationWithError:nil];
}

// Imports `credentials` into `_passkeyStore`, removes migrated and invalid
// entries from `temporalStore`, and returns newly created passkeys. Requires a
// non-null, ready `_passkeyStore`.
- (std::vector<sync_pb::WebauthnCredentialSpecifics>)
    importPasskeyCredentials:(NSArray<id<Credential>>*)credentials
                   fromStore:(UserDefaultsCredentialStore*)temporalStore {
  CHECK(_passkeyStore);
  CHECK(_passkeyStore->IsReady());

  base::flat_set<std::string> syncIDs = _passkeyStore->GetAllSyncIds();
  std::vector<sync_pb::WebauthnCredentialSpecifics> createdPasskeys;

  for (id<Credential> credential in credentials) {
    if (syncIDs.contains(StringViewFromData(credential.syncId))) {
      // If the passkey already exists, update its state only if the credential
      // is still active.
      if (!UpdateExistingPasskey(*_passkeyStore, credential)) {
        continue;
      }
    } else {
      sync_pb::WebauthnCredentialSpecifics passkey =
          PasskeyFromCredential(credential);
      if (webauthn::passkey_model_utils::IsGpmPasskeyValid(passkey)) {
        _passkeyStore->CreatePasskey(passkey);
        createdPasskeys.push_back(std::move(passkey));
        LogPasskeyMigrationStatus(PasskeysMigrationStatus::kPasskeyCreated);
      } else {
        LogPasskeyMigrationStatus(PasskeysMigrationStatus::kInvalidPasskey);
      }
    }
    [temporalStore
        removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  }

  return createdPasskeys;
}

// Schedules a fresh store pass to import deferred passkeys once `_passkeyStore`
// becomes ready.
- (void)migrateDeferredPasskeysIfReady {
  // `_passkeyStore` may have failed initialization, shut down, or been
  // cancelled after the readiness task was posted.
  if (!_pendingCompletion || _aborted || !_waitingForPasskeyModel) {
    return;
  }
  if (!_passkeyStore || !_passkeyStore->IsReady()) {
    _passkeyModelObserverBridge.reset();
    _passkeyStore = nullptr;
    _waitingForPasskeyModel = NO;
    if (_passState == StorePassState::kIdle) {
      [self completeMigrationWithError:nil];
    }
    return;
  }

  _waitingForPasskeyModel = NO;
  [self enqueueStorePass];
}

// Invokes `_pendingCompletion` with `error`, or with the passkeys created
// across all passes of the current migration if `error` is nil.
- (void)completeMigrationWithError:(NSError*)error {
  CHECK(_pendingCompletion);
  _waitingForPasskeyModel = NO;
  _needsFollowUpPass = NO;
  std::vector<sync_pb::WebauthnCredentialSpecifics> createdPasskeys =
      std::exchange(_accumulatedCreatedPasskeys, {});
  CredentialMigrationResult result =
      error ? CredentialMigrationResult(base::unexpected(error))
            : CredentialMigrationResult(std::move(createdPasskeys));
  std::move(_pendingCompletion).Run(std::move(result));
}

@end
