// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"

#import "base/functional/callback.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/legacy_session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_migration.h"
#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/web/model/session_state/web_session_state_cache_factory.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {

// Alias for readability.
using RequestedStorageFormat = SessionRestorationServiceFactory::StorageFormat;

// Value taken from Desktop Chrome.
constexpr base::TimeDelta kSaveDelay = base::Seconds(2.5);

// Returns the value of the session storage format pref.
SessionStorageFormat GetSessionStorageFormatPref(PrefService* prefs) {
  switch (prefs->GetInteger(kSessionStorageFormatPref)) {
    case base::to_underlying(SessionStorageFormat::kLegacy):
      return SessionStorageFormat::kLegacy;

    case base::to_underlying(SessionStorageFormat::kOptimized):
      return SessionStorageFormat::kOptimized;

    default:
      return SessionStorageFormat::kUnknown;
  }
}

// Returns the value of the session storage migration status pref.
SessionStorageMigrationStatus GetSessionStorageMigrationStatusPref(
    PrefService* prefs) {
  switch (prefs->GetInteger(kSessionStorageMigrationStatusPref)) {
    case base::to_underlying(SessionStorageMigrationStatus::kSuccess):
      return SessionStorageMigrationStatus::kSuccess;

    case base::to_underlying(SessionStorageMigrationStatus::kFailure):
      return SessionStorageMigrationStatus::kFailure;

    case base::to_underlying(
        SessionStorageMigrationStatus::kInProgressToLegacy):
      return SessionStorageMigrationStatus::kInProgressToLegacy;

    case base::to_underlying(
        SessionStorageMigrationStatus::kInProgressToOptimized):
      return SessionStorageMigrationStatus::kInProgressToOptimized;

    default:
      return SessionStorageMigrationStatus::kUnkown;
  }
}

// Returns whether `format` corresponds to `requested_format`.
bool IsSessionStorageInRequestedFormat(
    SessionStorageFormat format,
    RequestedStorageFormat requested_format) {
  switch (requested_format) {
    case RequestedStorageFormat::kLegacy:
      return format == SessionStorageFormat::kLegacy;

    case RequestedStorageFormat::kOptimized:
      return format == SessionStorageFormat::kOptimized;
  }
}

// Invoked when the session migration completes.
void OnSessionMigrationDone(
    base::WeakPtr<ChromeBrowserState> weak_browser_state,
    RequestedStorageFormat requested_format,
    base::Time migration_start,
    base::OnceClosure closure,
    ios::sessions::MigrationStatus status) {
  ChromeBrowserState* browser_state = weak_browser_state.get();
  if (!browser_state) {
    return;
  }

  PrefService* prefs = browser_state->GetPrefs();
  if (status == ios::sessions::MigrationStatus::kSuccess) {
    prefs->SetInteger(
        kSessionStorageFormatPref,
        base::to_underlying(requested_format == RequestedStorageFormat::kLegacy
                                ? SessionStorageFormat::kLegacy
                                : SessionStorageFormat::kOptimized));
    prefs->SetInteger(
        kSessionStorageMigrationStatusPref,
        base::to_underlying(SessionStorageMigrationStatus::kSuccess));
  } else {
    prefs->SetInteger(
        kSessionStorageFormatPref,
        base::to_underlying(requested_format == RequestedStorageFormat::kLegacy
                                ? SessionStorageFormat::kOptimized
                                : SessionStorageFormat::kLegacy));
    prefs->SetInteger(
        kSessionStorageMigrationStatusPref,
        base::to_underlying(SessionStorageMigrationStatus::kFailure));
  }

  std::move(closure).Run();
}

}  // namespace

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SessionRestorationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SessionRestorationServiceFactory*
SessionRestorationServiceFactory::GetInstance() {
  static base::NoDestructor<SessionRestorationServiceFactory> instance;
  return instance.get();
}

SessionRestorationServiceFactory::SessionRestorationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionRestorationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebSessionStateCacheFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

void SessionRestorationServiceFactory::MigrateSessionStorageFormat(
    ChromeBrowserState* browser_state,
    StorageFormat requested_format,
    base::OnceClosure closure) {
  DCHECK(!browser_state->IsOffTheRecord());
  DCHECK(!GetServiceForBrowserState(browser_state, false));

  PrefService* const prefs = browser_state->GetPrefs();

  // If the storage is already in the requested format, there is nothing
  // to do. Mark the migration as successful (to allow switching back to
  // the other format at a later point in time) and skip the migration.
  // Do not record any metric since migration was not attempted.
  const SessionStorageFormat format = GetSessionStorageFormatPref(prefs);
  if (IsSessionStorageInRequestedFormat(format, requested_format)) {
    prefs->SetInteger(
        kSessionStorageMigrationStatusPref,
        base::to_underlying(SessionStorageMigrationStatus::kSuccess));

    return std::move(closure).Run();
  }

  const SessionStorageMigrationStatus status =
      GetSessionStorageMigrationStatusPref(prefs);
  switch (status) {
    // The application crashed during the previous migration attempt. Record
    // the failure, update the storage format (it has to be the opposite of
    // the requested format otherwise the migration would have been a succes)
    // and skip the migration.
    case SessionStorageMigrationStatus::kInProgressToLegacy:
    case SessionStorageMigrationStatus::kInProgressToOptimized:
      prefs->SetInteger(
          kSessionStorageMigrationStatusPref,
          base::to_underlying(SessionStorageMigrationStatus::kFailure));
      prefs->SetInteger(
          kSessionStorageFormatPref,
          status == SessionStorageMigrationStatus::kInProgressToLegacy
              ? base::to_underlying(SessionStorageFormat::kOptimized)
              : base::to_underlying(SessionStorageFormat::kLegacy));
      return std::move(closure).Run();

    // The previous migration attempt was a failure, do not retry.
    case SessionStorageMigrationStatus::kFailure:
      return std::move(closure).Run();

    // If the previous migration attemtp was a success, then the format of
    // the storage has to be known (the requested format of that migration).
    case SessionStorageMigrationStatus::kSuccess:
      DCHECK_NE(format, SessionStorageFormat::kUnknown);
      break;

    // If the migration status is unknown, then it is the first time this
    // code is invoked, the format of the storage must be unknown too.
    case SessionStorageMigrationStatus::kUnkown:
      DCHECK_EQ(format, SessionStorageFormat::kUnknown);
      break;
  }

  // The migration is required. Update the migration status to "in progress"
  // and start the asynchronous migration on a background sequence.
  prefs->SetInteger(
      kSessionStorageMigrationStatusPref,
      base::to_underlying(
          requested_format == StorageFormat::kLegacy
              ? SessionStorageMigrationStatus::kInProgressToLegacy
              : SessionStorageMigrationStatus::kInProgressToOptimized));

  // Migrate all session in `browser_state`'s and OTR state paths.
  std::vector<base::FilePath> paths = {
      browser_state->GetStatePath(),
      browser_state->GetOffTheRecordStatePath(),
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock{}},
      base::BindOnce(requested_format == StorageFormat::kLegacy
                         ? &ios::sessions::MigrateSessionsInPathsToLegacy
                         : &ios::sessions::MigrateSessionsInPathsToOptimized,
                     std::move(paths)),
      base::BindOnce(&OnSessionMigrationDone, browser_state->AsWeakPtr(),
                     requested_format, base::Time::Now(), std::move(closure)));
}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  const base::FilePath storage_path = browser_state->GetStatePath();

  // If the optimised session restoration format is not enabled, create a
  // LegacySessionRestorationService instance which wraps the legacy API.
  if (!web::features::UseSessionSerializationOptimizations()) {
    SessionServiceIOS* session_service_ios =
        [[SessionServiceIOS alloc] initWithSaveDelay:kSaveDelay
                                          taskRunner:task_runner];

    return std::make_unique<LegacySessionRestorationService>(
        IsPinnedTabsEnabled(), storage_path, session_service_ios,
        WebSessionStateCacheFactory::GetForBrowserState(browser_state),
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
  }

  return std::make_unique<SessionRestorationServiceImpl>(
      kSaveDelay, IsPinnedTabsEnabled(), storage_path, task_runner,
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
}

web::BrowserState* SessionRestorationServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}

void SessionRestorationServiceFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kSessionStorageFormatPref,
      base::to_underlying(SessionStorageFormat::kUnknown));
  registry->RegisterIntegerPref(
      kSessionStorageMigrationStatusPref,
      base::to_underlying(SessionStorageMigrationStatus::kUnkown));
}
