// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "base/files/file_util.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/model/legacy_session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_migration.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_impl.h"
#import "ios/chrome/browser/sessions/model/session_service_ios.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state_id.h"

namespace {

// Alias for readability.
using RequestedStorageFormat = SessionRestorationServiceFactory::StorageFormat;

// Threshold before retrying to migration the session storage.
constexpr base::TimeDelta kRetryMigrationThreshold = base::Days(3);

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

    case base::to_underlying(SessionStorageMigrationStatus::kInProgress):
      return SessionStorageMigrationStatus::kInProgress;

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

// Converts `requested_format` to SessionStorageFormat according to the
// status of the migration operation.
SessionStorageFormat SessionStorageFormatFromRequestedFormat(
    RequestedStorageFormat requested_format,
    ios::sessions::MigrationResult::Status status) {
  switch (status) {
    case ios::sessions::MigrationResult::Status::kSuccess:
      return requested_format == RequestedStorageFormat::kLegacy
                 ? SessionStorageFormat::kLegacy
                 : SessionStorageFormat::kOptimized;

    case ios::sessions::MigrationResult::Status::kFailure:
      return requested_format != RequestedStorageFormat::kLegacy
                 ? SessionStorageFormat::kLegacy
                 : SessionStorageFormat::kOptimized;
  }
}

// Store the session storage format `storage_format` metric.
void RecordSessionStorageFormatMetric(SessionStorageFormat storage_format) {
  base::UmaHistogramEnumeration(
      kSessionHistogramStorageFormat,
      storage_format == SessionStorageFormat::kLegacy
          ? SessionHistogramStorageFormat::kLegacy
          : SessionHistogramStorageFormat::kOptimized);
}

// Store the session storage format `storage_format` and the migration
// status `migration_status` metrics.
void RecordSessionStorageFormatAndMigrationStatusMetrics(
    SessionStorageFormat storage_format,
    SessionStorageMigrationStatus migration_status) {
  RecordSessionStorageFormatMetric(storage_format);

  SessionHistogramStorageMigrationStatus histogram_status;
  switch (migration_status) {
    case SessionStorageMigrationStatus::kSuccess:
      histogram_status = SessionHistogramStorageMigrationStatus::kSuccess;
      break;

    case SessionStorageMigrationStatus::kFailure:
      histogram_status = SessionHistogramStorageMigrationStatus::kFailure;
      break;

    case SessionStorageMigrationStatus::kInProgress:
      histogram_status = SessionHistogramStorageMigrationStatus::kInterrupted;
      break;

    case SessionStorageMigrationStatus::kUnkown:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(kSessionHistogramStorageMigrationStatus,
                                histogram_status);
}

// Store the session storage format `storage_format` and the migration
// status `migration_status` to `pref_service`.
void RecordSessionStorageFormatAndMigrationStatus(
    PrefService* pref_service,
    SessionStorageFormat storage_format,
    SessionStorageMigrationStatus migration_status) {
  pref_service->SetInteger(kSessionStorageFormatPref,
                           base::to_underlying(storage_format));

  pref_service->SetInteger(kSessionStorageMigrationStatusPref,
                           base::to_underlying(migration_status));
}

// Detects the storage format for session at `path`. If no existing storage
// is found, return that the storage corresponds to `requested_format`.
SessionStorageFormat DetectStorageFormat(
    const base::FilePath& path,
    RequestedStorageFormat requested_format) {
  if (base::DirectoryExists(path.Append(kSessionRestorationDirname))) {
    return SessionStorageFormat::kOptimized;
  }

  if (base::DirectoryExists(path.Append(kLegacySessionsDirname))) {
    return SessionStorageFormat::kLegacy;
  }

  return SessionStorageFormatFromRequestedFormat(
      requested_format, ios::sessions::MigrationResult::Status::kSuccess);
}

// Invoked when the session storage format has been detected.
void OnStorageFormatDetected(base::WeakPtr<ProfileIOS> weak_profile,
                             base::OnceClosure closure,
                             SessionStorageFormat storage_format) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  RecordSessionStorageFormatAndMigrationStatus(
      profile->GetPrefs(), storage_format,
      SessionStorageMigrationStatus::kSuccess);

  RecordSessionStorageFormatMetric(storage_format);

  std::move(closure).Run();
}

// Invoked when the session migration completes.
void OnSessionMigrationDone(base::WeakPtr<ProfileIOS> weak_profile,
                            RequestedStorageFormat requested_format,
                            base::TimeTicks migration_start,
                            int32_t next_session_identifier,
                            base::OnceClosure closure,
                            ios::sessions::MigrationResult result) {
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  const SessionStorageMigrationStatus migration_status =
      result.status == ios::sessions::MigrationResult::Status::kSuccess
          ? SessionStorageMigrationStatus::kSuccess
          : SessionStorageMigrationStatus::kFailure;

  if (result.status == ios::sessions::MigrationResult::Status::kSuccess) {
    DCHECK_GE(result.next_session_identifier, next_session_identifier);
    if (result.next_session_identifier != next_session_identifier) {
      const int count =
          result.next_session_identifier - next_session_identifier;
      for (int i = 0; i < count; ++i) {
        std::ignore = web::WebStateID::NewUnique();
      }
    }
  }

  const SessionStorageFormat storage_format =
      SessionStorageFormatFromRequestedFormat(requested_format, result.status);

  RecordSessionStorageFormatAndMigrationStatus(
      profile->GetPrefs(), storage_format, migration_status);

  RecordSessionStorageFormatAndMigrationStatusMetrics(storage_format,
                                                      migration_status);

  base::UmaHistogramTimes(kSessionHistogramStorageMigrationTiming,
                          base::TimeTicks::Now() - migration_start);

  std::move(closure).Run();
}

}  // namespace

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForBrowserState(
    ProfileIOS* profile) {
  return GetForProfile(profile);
}

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<SessionRestorationService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
}

void SessionRestorationServiceFactory::MigrateSessionStorageFormat(
    ProfileIOS* profile,
    StorageFormat requested_format,
    base::OnceClosure closure) {
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(!GetServiceForBrowserState(profile, false));

  PrefService* const prefs = profile->GetPrefs();
  const SessionStorageMigrationStatus status =
      GetSessionStorageMigrationStatusPref(prefs);

  // Nothing to do, the storage is already in the requested format.
  const SessionStorageFormat format = GetSessionStorageFormatPref(prefs);
  if (IsSessionStorageInRequestedFormat(format, requested_format)) {
    // It is possible for status to not be "success" if the flag controlling
    // `requested_format` changed between invocation. In that case migration
    // would have been attempted in the previous run and failed. If this is
    // the case, then reset the status to "success" to allow attempting the
    // migration if the flag is flipped again in the future.
    if (status != SessionStorageMigrationStatus::kSuccess) {
      RecordSessionStorageFormatAndMigrationStatus(
          prefs, format, SessionStorageMigrationStatus::kSuccess);
    }

    RecordSessionStorageFormatAndMigrationStatusMetrics(
        format, SessionStorageMigrationStatus::kSuccess);

    return std::move(closure).Run();
  }

  // If the format is unknown, do not try to migrate, instead detect which
  // format is used and stay on the existing format. The migration will be
  // attempted on next application restart if necessary.
  if (format == SessionStorageFormat::kUnknown) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock{}},
        base::BindOnce(&DetectStorageFormat, profile->GetStatePath(),
                       requested_format),
        base::BindOnce(&OnStorageFormatDetected, profile->AsWeakPtr(),
                       std::move(closure)));
    return;
  }

  // The status should only be "unknown" on the first run, or when upgrading
  // from M-121 or earlier. In both case, the format would also be "unknown"
  // and the storage format detection logic called which will set the status
  // to "success". This means that neither the format nor the status can be
  // "unknown" when reaching this point.
  DCHECK_NE(format, SessionStorageFormat::kUnknown);
  DCHECK_NE(status, SessionStorageMigrationStatus::kUnkown);

  if (status == SessionStorageMigrationStatus::kFailure ||
      status == SessionStorageMigrationStatus::kInProgress) {
    // The previous attempt either failed, or was interrupted (either by the
    // user or due to a crash). Retry the migration if enough time has passed
    // since the last attempt (hopefully the reason that caused it to fail or
    // to be interrupted has changed), otherwise skip the migration and log
    // the failure.
    const base::Time last_attempt_time =
        prefs->GetTime(kSessionStorageMigrationStartedTimePref);

    if (base::Time::Now() - last_attempt_time < kRetryMigrationThreshold) {
      RecordSessionStorageFormatAndMigrationStatusMetrics(format, status);
      return std::move(closure).Run();
    }
  }

  // The migration is required. Update the migration status to "in progress"
  // and start the asynchronous migration on a background sequence. Record
  // the time of the migration start in order to periodically retry it in
  // case of failure.
  prefs->SetInteger(
      kSessionStorageMigrationStatusPref,
      base::to_underlying(SessionStorageMigrationStatus::kInProgress));
  prefs->SetTime(kSessionStorageMigrationStartedTimePref, base::Time::Now());

  // Migrate all session in `profile`'s and OTR state paths.
  std::vector<base::FilePath> paths = {
      profile->GetStatePath(),
      profile->GetOffTheRecordStatePath(),
  };

  const web::WebStateID identifier = web::WebStateID::NewUnique();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock{}},
      base::BindOnce(requested_format == StorageFormat::kLegacy
                         ? &ios::sessions::MigrateSessionsInPathsToLegacy
                         : &ios::sessions::MigrateSessionsInPathsToOptimized,
                     std::move(paths), identifier.identifier()),
      base::BindOnce(&OnSessionMigrationDone, profile->AsWeakPtr(),
                     requested_format, base::TimeTicks::Now(),
                     identifier.identifier(), std::move(closure)));
}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  const base::FilePath storage_path = profile->GetStatePath();

  SessionStorageFormat format =
      GetSessionStorageFormatPref(profile->GetPrefs());

  // During unit tests, it is the method MigrateSessionStorageFormat(...)
  // will not be called before the service is created and the preference
  // will have its default value of `SessionStorageFormat::kUnknown`. Use
  // the feature flag to select which implementation to use.
  if (format == SessionStorageFormat::kUnknown) {
    format = SessionStorageFormat::kOptimized;
  }

  // If the optimised session restoration format is not enabled, create a
  // LegacySessionRestorationService instance which wraps the legacy API.
  if (format == SessionStorageFormat::kLegacy) {
    SessionServiceIOS* session_service_ios =
        [[SessionServiceIOS alloc] initWithSaveDelay:kSaveDelay
                                          taskRunner:task_runner];

    return std::make_unique<LegacySessionRestorationService>(
        IsPinnedTabsEnabled(), IsTabGroupInGridEnabled(), storage_path,
        session_service_ios,
        WebSessionStateCacheFactory::GetForProfile(profile));
  }

  return std::make_unique<SessionRestorationServiceImpl>(
      kSaveDelay, IsPinnedTabsEnabled(), IsTabGroupInGridEnabled(),
      storage_path, task_runner);
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
  registry->RegisterTimePref(kSessionStorageMigrationStartedTimePref,
                             base::Time());
}
