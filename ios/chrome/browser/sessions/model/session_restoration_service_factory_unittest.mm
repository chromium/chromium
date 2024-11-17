// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Configures preferences storing session storage format and session storage
// migration status.
void WriteSessionStoragePref(PrefService* prefs,
                             SessionStorageFormat storage_format,
                             SessionStorageMigrationStatus migration_status,
                             base::Time last_migration_attempt_time) {
  prefs->SetInteger(kSessionStorageFormatPref,
                    base::to_underlying(storage_format));
  prefs->SetInteger(kSessionStorageMigrationStatusPref,
                    base::to_underlying(migration_status));
  prefs->SetTime(kSessionStorageMigrationStartedTimePref,
                 last_migration_attempt_time);
}

// Checks preferences storing session storage format and session storage
// migration status have expected values.
void CheckSessionStoragePref(PrefService* prefs,
                             SessionStorageFormat storage_format,
                             SessionStorageMigrationStatus migration_status) {
  EXPECT_EQ(prefs->GetInteger(kSessionStorageFormatPref),
            base::to_underlying(storage_format));
  EXPECT_EQ(prefs->GetInteger(kSessionStorageMigrationStatusPref),
            base::to_underlying(migration_status));
}

// Name of the session used by the tests (random string obtained by
// running `uuidgen` on the command-line, no meaning to it).
const char kSessionIdentifier[] = "2D357BD5-2867-482F-A164-E7EF8EEED6AF";

// Returns the path of the legacy session named `identifier` in `root`.
base::FilePath LegacySessionPath(const base::FilePath& root,
                                 const std::string& identifier) {
  return root.Append(kLegacySessionsDirname)
      .Append(identifier)
      .Append(kLegacySessionFilename);
}

// Creates an empty session in legacy format named `identifier` in `root`.
bool CreateLegacySession(const base::FilePath& root,
                         const std::string& identifier) {
  SessionWindowIOS* session =
      [[SessionWindowIOS alloc] initWithSessions:@[]
                                       tabGroups:@[]
                                   selectedIndex:NSNotFound];

  const base::FilePath session_path = LegacySessionPath(root, identifier);
  return ios::sessions::WriteSessionWindow(session_path, session);
}

// Returns whether a legacy session named `identifier` exists in `root`.
bool LegacySessionExists(const base::FilePath& root,
                         const std::string& identifier) {
  const base::FilePath session_path = LegacySessionPath(root, identifier);
  return ios::sessions::ReadSessionWindow(session_path) != nil;
}

// Returns the path of the optimized session named `identifier` in `root`.
base::FilePath OptimizedSessionPath(const base::FilePath& root,
                                    const std::string& identifier) {
  return root.Append(kSessionRestorationDirname)
      .Append(identifier)
      .Append(kSessionMetadataFilename);
}

// Creates an empty session in optimized format named `identifier` in `root`.
bool CreateOptimizedSession(const base::FilePath& root,
                            const std::string& identifier) {
  ios::proto::WebStateListStorage session_storage;
  session_storage.set_active_index(-1);

  const base::FilePath session_path = OptimizedSessionPath(root, identifier);
  return ios::sessions::WriteProto(session_path, session_storage);
}

// Returns whether a legacy session named `identifier` exists in `root`.
bool OptimizedSessionExists(const base::FilePath& root,
                            const std::string& identifier) {
  ios::proto::WebStateListStorage session_storage;
  const base::FilePath session_path = OptimizedSessionPath(root, identifier);
  return ios::sessions::ParseProto(session_path, session_storage);
}

}  // namespace

class SessionRestorationServiceFactoryTest : public PlatformTest {
 public:
  SessionRestorationServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  ProfileIOS* profile() { return profile_.get(); }

  ProfileIOS* otr_profile() { return profile_->GetOffTheRecordProfile(); }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  base::HistogramTester histogram_tester_;
};

// Tests that the factory correctly instantiate a new service when the storage
// format is "unknown".
TEST_F(SessionRestorationServiceFactoryTest, CreateInstance_Unknown) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(profile()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// BrowserState when the storage format is "unknown".
TEST_F(SessionRestorationServiceFactoryTest, CreateOTRInstance_Unknown) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that regular and off-the-record BrowserState uses distinct instances
// when the storage format is "unknown".
TEST_F(SessionRestorationServiceFactoryTest, InstancesAreDistinct_Unknown) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_NE(SessionRestorationServiceFactory::GetForProfile(profile()),
            SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that the factory correctly instantiate a new service when using
// the "legacy" storage.
TEST_F(SessionRestorationServiceFactoryTest, CreateInstance_Legacy) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(profile()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// BrowserState when using the "legacy" storage.
TEST_F(SessionRestorationServiceFactoryTest, CreateOTRInstance_Legacy) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that regular and off-the-record BrowserState uses distinct instances
// when using the "legacy" storage.
TEST_F(SessionRestorationServiceFactoryTest, InstancesAreDistinct_Legacy) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  EXPECT_NE(SessionRestorationServiceFactory::GetForProfile(profile()),
            SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that the factory correctly instantiate a new service when using
// the "optimized" storage.
TEST_F(SessionRestorationServiceFactoryTest, CreateInstance_Optimized) {
  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(profile()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// BrowserState when using the "optimized" storage.
TEST_F(SessionRestorationServiceFactoryTest, CreateOTRInstance_Optimized) {
  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  EXPECT_TRUE(SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that regular and off-the-record BrowserState uses distinct instances
// when using the "optimized" storage.
TEST_F(SessionRestorationServiceFactoryTest, InstancesAreDistinct_Optimized) {
  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  EXPECT_NE(SessionRestorationServiceFactory::GetForProfile(profile()),
            SessionRestorationServiceFactory::GetForProfile(otr_profile()));
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format known to be in legacy and check
// the operation is synchronous.
TEST_F(SessionRestorationServiceFactoryTest, MigrateSession_ToLegacy_Legacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy, std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When no
// previous session exists, and thus the detection must succeed, and reports
// the storage is in the requested format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_UnknownInexistent) {
  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When a
// previous session in legacy format exists, it should leave it untouched,
// the detection should report success, and that the storage is in legacy
// format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_UnknownAsLegacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When a
// previous session in optimized format exists, it should leave it untouched,
// the detection should report success, and that the storage is in optimized
// format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_UnknownAsOptimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format is in optimized format, and thus
// requires conversion.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_Optimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) will mark the migration as failed
// if it cannot convert the storage from optimized to legacy.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedFailureMigration) {
  // Write a broken session in optimized format.
  const base::FilePath root = profile()->GetStatePath();
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath path = OptimizedSessionPath(root, kSessionIdentifier);
  ASSERT_TRUE(ios::sessions::WriteFile(path, data));

  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the legacy session has not been created, and that the old
  // corrupt session is still present.
  EXPECT_FALSE(LegacySessionExists(root, kSessionIdentifier));
  EXPECT_NSEQ(ios::sessions::ReadFile(path), data);

  // Check that the preferences have been updated, and the migration marked
  // as failed.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kFailure, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to legacy but the previous migration failed.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedPreviousMigrationFailed) {
  WriteSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure,
                          base::Time::Now() - base::Hours(1));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy, std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kFailure, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to legacy but the application crashed while
// the previous migration was in progress.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedPreviousMigrationCrashedInProgress) {
  WriteSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kInProgress,
                          base::Time::Now() - base::Hours(1));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy, std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kInProgress);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(base::Bucket(
          SessionHistogramStorageMigrationStatus::kInterrupted, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) retry the migration from optimized
// to migration if the previous attempt happened a while ago but failed.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedRetryMigrationFailed) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure,
                          base::Time::Now() - base::Days(7));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) retry the migration from optimized
// to migration if the previous attempt happened a while ago but was
// interrupted.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedRetryMigrationCrashedInProgress) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kInProgress,
                          base::Time::Now() - base::Days(7));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}
// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format known to be in optimized and check
// the operation is synchronous.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_Optimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(
      profile()->GetPrefs(), SessionStorageFormat::kOptimized,
      SessionStorageMigrationStatus::kSuccess, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When no
// previous session exists, and thus the detection must succeed, and reports
// the storage is in the requested format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_UnknownInexistent) {
  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When a
// previous session in legacy format exists, it should leave it untouched,
// the detection should report success, and that the storage is in legacy
// format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_UnknownAsOptimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does not perform conversion when the
// storage format is unknown, and instead detect the existing format. When a
// previous session in optimized format exists, it should leave it untouched,
// the detection should report success, and that the storage is in optimized
// format.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_UnknownAsLegacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown, base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre());

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format is in legacy format, and thus
// requires conversion.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_Legacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) will mark the migration as failed
// if it cannot convert the storage from legacy to optimized.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyFailureMigration) {
  // Write a broken session in legacy format.
  const base::FilePath root = profile()->GetStatePath();
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath path = LegacySessionPath(root, kSessionIdentifier);
  ASSERT_TRUE(ios::sessions::WriteFile(path, data));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess,
                          base::Time());

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the optimized session has not been created, and that the old
  // corrupt session is still present.
  EXPECT_FALSE(OptimizedSessionExists(root, kSessionIdentifier));
  EXPECT_NSEQ(ios::sessions::ReadFile(path), data);

  // Check that the preferences have been updated, and the migration marked
  // as failed.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kFailure, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to optimized but the previous migration failed.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyPreviousMigrationFailed) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure,
                          base::Time::Now() - base::Hours(1));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kFailure, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to optimized but the application crashed while
// the previous migration was in progress.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyPreviousMigrationCrashedInProgress) {
  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kInProgress,
                          base::Time::Now() - base::Hours(1));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kInProgress);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kLegacy, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(base::Bucket(
          SessionHistogramStorageMigrationStatus::kInterrupted, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      base::BucketsAre());
}

// Tests that MigrateSessionStorage(...) retry the migration from optimized
// to migration if the previous attempt happened a while ago but failed.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyRetryMigrationFailed) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure,
                          base::Time::Now() - base::Days(7));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format is in legacy format, and thus
// requires conversion.
TEST_F(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyRetryMigrationCrashedInProgress) {
  // Create an empty session in legacy format.
  const base::FilePath& root = profile()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(profile()->GetPrefs(), SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kInProgress,
                          base::Time::Now() - base::Days(7));

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      profile(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(profile()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  // Check that the expected metrics have been recorded.
  EXPECT_THAT(histogram_tester().GetAllSamples(kSessionHistogramStorageFormat),
              base::BucketsAre(
                  base::Bucket(SessionHistogramStorageFormat::kOptimized, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationStatus),
      base::BucketsAre(
          base::Bucket(SessionHistogramStorageMigrationStatus::kSuccess, 1)));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(kSessionHistogramStorageMigrationTiming),
      testing::Not(base::BucketsAre()));
}
