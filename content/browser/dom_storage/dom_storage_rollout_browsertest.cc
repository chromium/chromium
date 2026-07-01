// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using storage::DomStorageSqliteRolloutStage;

// On desktop the storage service runs out-of-process and sandboxed, so all
// filesystem access must be brokered through a `FilesystemProxy`. Raw `base::`
// filesystem calls are blocked in the sandbox and fail silently, misclassifying
// databases. These tests guard the SQLite-rollout disk checks against that
// silent failure. The dom_storage unit tests run in-process and unsandboxed, so
// they cannot catch it.
//
// On Android the storage service runs in-process and unsandboxed. There, these
// tests ensure that our rollout's disk access dependent logic functions
// correctly.
class DOMStorageRolloutBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<DomStorageSqliteRolloutStage> {
 public:
  DOMStorageRolloutBrowserTest() {
    // PRE_ runs lay down a plain, untagged LevelDB with the LevelDb-only stage,
    // mimicking a Profile from before the rollout. All other runs use the
    // experimental arm under test.
    const DomStorageSqliteRolloutStage stage =
        IsPreTest() ? DomStorageSqliteRolloutStage::kUseLevelDbOnly
                    : GetParam();
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{storage::kDomStorageSqliteNewDatabases,
                               {{"DomStorageSqliteNewDatabasesStage",
                                 storage::kDomStorageSqliteNewDatabasesStage
                                     .GetName(stage)}}}},
        /*disabled_features=*/{storage::kDomStorageSqlite,
                               storage::kDomStorageSqliteInMemory});
  }

 protected:
  bool IsControlArm() const {
    return GetParam() == DomStorageSqliteRolloutStage::kUseLevelDbAsControl;
  }

  StoragePartition* partition() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

  base::FilePath LevelDbDir() {
    return partition()
        ->GetPath()
        .AppendASCII("Local Storage")
        .AppendASCII("leveldb");
  }

  base::FilePath ExpTagPath() { return LevelDbDir().AppendASCII("exp-v1"); }

  base::FilePath SqliteDbPath() {
    return partition()->GetPath().AppendASCII("LocalStorage");
  }

  // Waits for the Local Storage database to finish opening.
  // `GetLocalStorageUsage` only replies once the connection has finished
  // opening, including any on-disk state check and experimental-tag write the
  // active rollout stage performs.
  void OpenLocalStorageDatabase() {
    base::RunLoop loop;
    partition()->GetDOMStorageContext()->GetLocalStorageUsage(
        base::BindLambdaForTesting(
            [&](const std::vector<StorageUsageInfo>&) { loop.Quit(); }));
    loop.Run();
  }

  // The Storage Service runs in-process on Android. LocalStorage opens during
  // startup. So the `OpenDatabase` sample would get missed if HistogramTester
  // were initialized inside the test body. On Desktop OSes the sample only
  // arrives in the Browser process after FetchHistogramsFromChildProcesses().
  base::HistogramTester histograms_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    DOMStorageRolloutBrowserTest,
    testing::Values(DomStorageSqliteRolloutStage::kUseLevelDbAsControl,
                    DomStorageSqliteRolloutStage::kUseSqliteForNewDatabases),
    [](const testing::TestParamInfo<DomStorageSqliteRolloutStage>& info) {
      return storage::kDomStorageSqliteNewDatabasesStage.GetName(info.param);
    });

// Verifies that a brand-new on-disk database is created with the backend the
// rollout arm selects, and that the control arm writes the experimental tag.
IN_PROC_BROWSER_TEST_P(DOMStorageRolloutBrowserTest, CreatesConfiguredBackend) {
#if BUILDFLAG(IS_FUCHSIA)
  // Fuchsia's sandboxed SQLite VFS cannot acquire file locks, so a new SQLite
  // database cannot be opened there. The control arm uses LevelDB and is
  // unaffected.
  // TODO(crbug.com/488731425): Re-enable the treatment arm once the sandboxed
  // DomStorage SQLite backend runs on Fuchsia.
  if (!IsControlArm()) {
    GTEST_SKIP() << "SQLite DomStorage backend unsupported on Fuchsia";
  }
#endif
  OpenLocalStorageDatabase();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (IsControlArm()) {
      // The control arm creates a LevelDB and tags it as experimental.
      EXPECT_TRUE(base::PathExists(LevelDbDir()));
      EXPECT_TRUE(base::PathExists(ExpTagPath()));
      EXPECT_FALSE(base::PathExists(SqliteDbPath()));
    } else {
      // The treatment arm creates a SQLite database for the new store.
      EXPECT_TRUE(base::PathExists(SqliteDbPath()));
      EXPECT_FALSE(base::PathExists(LevelDbDir()));
    }
  }

  FetchHistogramsFromChildProcesses();
  // A newly-created on-disk database is attributed to the experiment in both
  // arms.
  histograms_.ExpectUniqueSample(
      "Storage.LocalStorage.OpenDatabase.OnDiskExperimental",
      /*sample=*/0, /*expected_bucket_count=*/1);
}

// Verifies that a pre-existing LevelDB database is reused as-is under the
// experimental arms. The database is created by a Profile from before the
// rollout. Its telemetry stays attributed to the pre-existing "OnDisk"
// population.
IN_PROC_BROWSER_TEST_P(DOMStorageRolloutBrowserTest,
                       PRE_KeepsPreExistingLevelDb) {
  OpenLocalStorageDatabase();

  base::ScopedAllowBlockingForTesting allow_blocking;
  // The pre-existing LevelDB shouldn't contain the experimental tag.
  ASSERT_TRUE(base::PathExists(LevelDbDir()));
  ASSERT_FALSE(base::PathExists(ExpTagPath()));
}

IN_PROC_BROWSER_TEST_P(DOMStorageRolloutBrowserTest, KeepsPreExistingLevelDb) {
  OpenLocalStorageDatabase();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // The pre-existing LevelDB is reused as-is: not abandoned for a new SQLite
    // database (treatment) and not retagged as experimental (control).
    EXPECT_TRUE(base::PathExists(LevelDbDir()));
    EXPECT_FALSE(base::PathExists(SqliteDbPath()));
    EXPECT_FALSE(base::PathExists(ExpTagPath()));
  }

  FetchHistogramsFromChildProcesses();
  histograms_.ExpectUniqueSample("Storage.LocalStorage.OpenDatabase.OnDisk",
                                 /*sample=*/0, /*expected_bucket_count=*/1);
  histograms_.ExpectTotalCount(
      "Storage.LocalStorage.OpenDatabase.OnDiskExperimental", 0);
}

}  // namespace
}  // namespace content
