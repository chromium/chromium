// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/model/legacy_session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_impl.h"
#import "ios/chrome/browser/sessions/model/session_service_ios.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace {

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

}  // namespace

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<SessionRestorationService>(
      profile, /*create=*/true);
}

// static
SessionRestorationServiceFactory*
SessionRestorationServiceFactory::GetInstance() {
  static base::NoDestructor<SessionRestorationServiceFactory> instance;
  return instance.get();
}

SessionRestorationServiceFactory::SessionRestorationServiceFactory()
    : ProfileKeyedServiceFactoryIOS("SessionRestorationService",
                                    ProfileSelection::kOwnInstanceInIncognito) {
  DependsOn(WebSessionStateCacheFactory::GetInstance());
}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
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
        IsPinnedTabsEnabled(), storage_path, session_service_ios,
        WebSessionStateCacheFactory::GetForProfile(profile));
  }

  return std::make_unique<SessionRestorationServiceImpl>(
      kSaveDelay, IsPinnedTabsEnabled(), storage_path, task_runner);
}

void SessionRestorationServiceFactory::RegisterProfilePrefs(
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
