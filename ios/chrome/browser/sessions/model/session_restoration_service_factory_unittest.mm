// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"

#import "base/types/cxx23_to_underlying.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
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

}  // namespace

class SessionRestorationServiceFactoryTest : public PlatformTest {
 public:
  SessionRestorationServiceFactoryTest()
      : profile_(TestProfileIOS::Builder().Build()) {}

  ProfileIOS* profile() { return profile_.get(); }

  ProfileIOS* otr_profile() { return profile_->GetOffTheRecordProfile(); }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
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
