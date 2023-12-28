// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {

struct SessionRestorationServiceFactoryTestParam {
  const bool enable_session_serialization_optimization;
};

constexpr SessionRestorationServiceFactoryTestParam
    kEnableSessionSerializationOptimization = {
        .enable_session_serialization_optimization = true,
};

constexpr SessionRestorationServiceFactoryTestParam
    kDisableSessionSerializationOptimization = {
        .enable_session_serialization_optimization = false,
};

// Configures preferences storing session storage format and session storage
// migration status.
void WriteSessionStoragePref(PrefService* prefs,
                             SessionStorageFormat storage_format,
                             SessionStorageMigrationStatus migration_status) {
  prefs->SetInteger(kSessionStorageFormatPref,
                    base::to_underlying(storage_format));
  prefs->SetInteger(kSessionStorageMigrationStatusPref,
                    base::to_underlying(migration_status));
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
      [[SessionWindowIOS alloc] initWithSessions:@[] selectedIndex:NSNotFound];

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

class SessionRestorationServiceFactoryTest
    : public testing::TestWithParam<SessionRestorationServiceFactoryTestParam> {
 public:
  SessionRestorationServiceFactoryTest() {
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    const SessionRestorationServiceFactoryTestParam param = GetParam();
    if (param.enable_session_serialization_optimization) {
      scoped_feature_list_->InitAndEnableFeature(
          web::features::kEnableSessionSerializationOptimizations);
    } else {
      scoped_feature_list_->InitAndDisableFeature(
          web::features::kEnableSessionSerializationOptimizations);
    }

    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
    browser_state_ = TestChromeBrowserState::Builder().Build();
  }

  ChromeBrowserState* browser_state() { return browser_state_.get(); }

  ChromeBrowserState* otr_browser_state() {
    return browser_state_->GetOffTheRecordChromeBrowserState();
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

INSTANTIATE_TEST_SUITE_P(
    SessionRestorationServiceFactoryTestWithFeatureSelection,
    SessionRestorationServiceFactoryTest,
    ::testing::Values(kEnableSessionSerializationOptimization,
                      kDisableSessionSerializationOptimization));

// Tests that the factory correctly instantiate a new service.
TEST_P(SessionRestorationServiceFactoryTest, CreateInstance) {
  EXPECT_TRUE(
      SessionRestorationServiceFactory::GetForBrowserState(browser_state()));
}

// Tests that the factory correctly instantiate a new service for off-the-record
// BrowserState.
TEST_P(SessionRestorationServiceFactoryTest, CreateOffTheRecordInstance) {
  EXPECT_TRUE(SessionRestorationServiceFactory::GetForBrowserState(
      otr_browser_state()));
}

// Tests that regular and off-the-record BrowserState uses distinct instances.
TEST_P(SessionRestorationServiceFactoryTest, InstancesAreDistinct) {
  EXPECT_NE(
      SessionRestorationServiceFactory::GetForBrowserState(browser_state()),
      SessionRestorationServiceFactory::GetForBrowserState(
          otr_browser_state()));
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format known to be in legacy and check
// the operation is synchronous.
TEST_P(SessionRestorationServiceFactoryTest, MigrateSession_ToLegacy_Legacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format is unknown, the sessions are in
// the legacy format, and thus the conversion is a no-op.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_UnknownAsLegacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format is unknown, the sessions are in
// the optimized format, and thus requires conversion.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_UnknownAsOptimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// legacy storage when the storage format is in optimized format, and thus
// requires conversion.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_Optimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the legacy format.
  EXPECT_TRUE(LegacySessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) will mark the migration as failed
// if it cannot convert the storage from optimized to legacy.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedFailureMigration) {
  // Write a broken session in optimized format.
  const base::FilePath root = browser_state()->GetStatePath();
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath path = OptimizedSessionPath(root, kSessionIdentifier);
  ASSERT_TRUE(ios::sessions::WriteFile(path, data));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
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
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to legacy but the previous migration failed.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedPreviousMigrationFailed) {
  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to legacy but the application crashed while
// the previous migration was in progress.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToLegacy_OptimizedPreviousMigrationCrashedInProgress) {
  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kInProgressToLegacy);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kLegacy,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kFailure);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format known to be in optimized and check
// the operation is synchronous.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_Optimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format is unknown, the sessions are in
// the optimized format, and thus the conversion is a no-op.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_UnknownAsOptimized) {
  // Create an empty session in optimized format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateOptimizedSession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format is unknown, the sessions are in
// the legacy format, and thus requires conversion.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_UnknownAsLegacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kUnknown,
                          SessionStorageMigrationStatus::kUnkown);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) succeed when asked to migrate to
// optimized storage when the storage format is in legacy format, and thus
// requires conversion.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_Legacy) {
  // Create an empty session in legacy format.
  const base::FilePath& root = browser_state()->GetStatePath();
  ASSERT_TRUE(CreateLegacySession(root, kSessionIdentifier));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure).Then(run_loop.QuitClosure()));
  EXPECT_FALSE(callback_called);

  // The callback should eventually be called, but asynchronously.
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  // Check that the session storage is in the optimized format.
  EXPECT_TRUE(OptimizedSessionExists(root, kSessionIdentifier));

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kOptimized,
                          SessionStorageMigrationStatus::kSuccess);
}

// Tests that MigrateSessionStorage(...) will mark the migration as failed
// if it cannot convert the storage from legacy to optimized.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyFailureMigration) {
  // Write a broken session in legacy format.
  const base::FilePath root = browser_state()->GetStatePath();
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  const base::FilePath path = LegacySessionPath(root, kSessionIdentifier);
  ASSERT_TRUE(ios::sessions::WriteFile(path, data));

  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kSuccess);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that it is not immediate, but requires
  // to spin the main run loop.
  base::RunLoop run_loop;
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
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
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to optimized but the previous migration failed.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyPreviousMigrationFailed) {
  WriteSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been not updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);
}

// Tests that MigrateSessionStorage(...) does nothing synchronously if
// asked to migrate session to optimized but the application crashed while
// the previous migration was in progress.
TEST_P(SessionRestorationServiceFactoryTest,
       MigrateSession_ToOptimized_LegacyPreviousMigrationCrashedInProgress) {
  WriteSessionStoragePref(
      browser_state()->GetPrefs(), SessionStorageFormat::kLegacy,
      SessionStorageMigrationStatus::kInProgressToOptimized);

  bool callback_called = false;
  base::OnceClosure closure =
      base::BindOnce([](bool* invoked) { *invoked = true; }, &callback_called);

  // Start the migration, and check that is is immediate and does not require
  // to sping the main run loop.
  SessionRestorationServiceFactory::GetInstance()->MigrateSessionStorageFormat(
      browser_state(), SessionRestorationServiceFactory::kOptimized,
      std::move(closure));
  EXPECT_TRUE(callback_called);

  // Check that the preferences have been updated.
  CheckSessionStoragePref(browser_state()->GetPrefs(),
                          SessionStorageFormat::kLegacy,
                          SessionStorageMigrationStatus::kFailure);
}
