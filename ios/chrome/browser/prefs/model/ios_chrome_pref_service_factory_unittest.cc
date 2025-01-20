// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/model/ios_chrome_pref_service_factory.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/features.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class CreateProfilePrefsTestBase : public PlatformTest {
 public:
  CreateProfilePrefsTestBase()
      : pref_registry_(
            base::MakeRefCounted<user_prefs::PrefRegistrySyncable>()) {
    EXPECT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  std::unique_ptr<sync_preferences::PrefServiceSyncable> BuildPrefService() {
    return CreateProfilePrefs(
        data_dir_.GetPath(), task_environment_.GetMainThreadTaskRunner().get(),
        pref_registry_, /*policy_service=*/nullptr,
        /*policy_connector=*/nullptr, /*supervised_user_prefs=*/nullptr,
        /*async=*/true);
  }

  base::FilePath AccountPreferencesFilePath() const {
    return data_dir_.GetPath().Append(kAccountPreferencesFilename);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
};

class CreateProfilePrefsTestWithMigrateAccountPrefsEnabled
    : public CreateProfilePrefsTestBase {
 public:
  CreateProfilePrefsTestWithMigrateAccountPrefsEnabled()
      : feature_list_(syncer::kMigrateAccountPrefs) {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CreateProfilePrefsTestWithMigrateAccountPrefsEnabled,
       ShouldRemoveAccountPrefsFile) {
  // Simulate a pre-existing account preferences file.
  ASSERT_TRUE(base::WriteFile(AccountPreferencesFilePath(), ""));

  BuildPrefService();
  // Wait for the posted task to delete the file finish.
  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  run_loop.Run();

  // Account prefs file should have been removed.
  EXPECT_FALSE(base::PathExists(AccountPreferencesFilePath()));
}

}  // namespace
