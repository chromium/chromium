// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class DeclarativePerformanceObserverStoreTest : public testing::Test {
 public:
  DeclarativePerformanceObserverStoreTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DeclarativePerformanceObserverStoreTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  std::unique_ptr<DeclarativePerformanceObserverStore> CreateStore() {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, temp_dir_.GetPath(), nullptr,
        run_loop.QuitClosure());
    run_loop.Run();
    return store;
  }

  std::unique_ptr<DeclarativePerformanceObserverStore> CreateStoreInMemory() {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/true, base::FilePath(), nullptr,
        run_loop.QuitClosure());
    run_loop.Run();
    return store;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(DeclarativePerformanceObserverStoreTest, StoragePolicyOptInAndOut) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStore();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));

  base::RunLoop run_loop2;
  store->SetEarlyFailurePolicy(kOrigin, false, run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, IncognitoModeInMemoryOnly) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStoreInMemory();

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, PersistsAcrossRestarts) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("TestProfile");

  // 1. Create store and set policy.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // 2. Re-create store with the same db_path and check persistence.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));
  }
}

TEST_F(DeclarativePerformanceObserverStoreTest, RaceConditionDuringLoad) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("TestProfile2");

  // Pre-populate DB with the policy.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // Start new store but DON'T wait for loading to complete yet.
  auto store = std::make_unique<DeclarativePerformanceObserverStore>(
      /*is_in_memory=*/false, profile_path, nullptr, base::DoNothing());

  // Instantly disable the policy before loading completes.
  base::RunLoop run_loop_set;
  store->SetEarlyFailurePolicy(kOrigin, false, run_loop_set.QuitClosure());

  // Wait for the SetEarlyFailurePolicy DB task (and the preceding load task) to
  // finish.
  run_loop_set.Run();

  // Now, cache should remain false even after the Load task completes,
  // preventing the old load results (which has kOrigin = true) from overwriting
  // it.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, OpaqueOriginIgnored) {
  const url::Origin kOpaqueOrigin = url::Origin();
  ASSERT_TRUE(kOpaqueOrigin.opaque());
  auto store = CreateStore();

  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(kOpaqueOrigin, true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOpaqueOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, DiskPersistence) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStore();
  base::HistogramTester histogram_tester;

  base::DictValue report;
  report.Set("test_key", "test_value");
  base::RunLoop run_loop_store;
  store->StoreEarlyFailureReport(kOrigin, report.Clone(),
                                 run_loop_store.QuitClosure());
  run_loop_store.Run();

  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.StoreReportResult",
      /*sample=*/0, /*expected_bucket_count=*/1);

  base::ListValue taken_reports;
  base::RunLoop run_loop_take;
  store->TakeEarlyFailureReports(
      kOrigin,
      base::BindOnce(
          [](base::OnceClosure quit, base::ListValue* out_taken_reports,
             base::ListValue result) {
            *out_taken_reports = std::move(result);
            std::move(quit).Run();
          },
          run_loop_take.QuitClosure(), &taken_reports));
  run_loop_take.Run();

  ASSERT_EQ(taken_reports.size(), 1u);
  const base::DictValue* dict = taken_reports[0].GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ(*(dict->FindString("test_key")), "test_value");

  // Taking reports a second time should return an empty list:
  base::ListValue taken_reports_empty;
  base::RunLoop run_loop_take2;
  store->TakeEarlyFailureReports(
      kOrigin,
      base::BindOnce(
          [](base::OnceClosure quit, base::ListValue* out_taken_reports,
             base::ListValue result) {
            *out_taken_reports = std::move(result);
            std::move(quit).Run();
          },
          run_loop_take2.QuitClosure(), &taken_reports_empty));
  run_loop_take2.Run();

  EXPECT_TRUE(taken_reports_empty.empty());
}

TEST_F(DeclarativePerformanceObserverStoreTest, DatabaseSchemaConfigured) {
  auto store = CreateStore();
  bool table_ok = false;
  bool index_ok = false;
  base::RunLoop run_loop;
  store->CheckSchemaForTesting(  // IN-TEST
      base::BindOnce(
          [](base::OnceClosure quit, bool* out_table, bool* out_index,
             bool table_res, bool index_res) {
            *out_table = table_res;
            *out_index = index_res;
            std::move(quit).Run();
          },
          run_loop.QuitClosure(), &table_ok, &index_ok));
  run_loop.Run();
  EXPECT_TRUE(table_ok);
  EXPECT_TRUE(index_ok);
}

TEST_F(DeclarativePerformanceObserverStoreTest, Enforces640KBFIFOQuota) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStoreInMemory();

  base::RunLoop run_loop_limit;
  store->SetQuotaLimitForTesting(4096, run_loop_limit.QuitClosure());
  run_loop_limit.Run();

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  base::DictValue sample;
  sample.Set("entryType", "navigation");
  sample.Set("name", kOrigin.GetURL().spec());
  sample.Set("padding", std::string(200, 'x'));

  for (int i = 0; i < 20; ++i) {
    base::DictValue report = sample.Clone();
    report.Set("index", i);
    store->StoreEarlyFailureReport(kOrigin, std::move(report));
  }

  base::ListValue reports;
  base::RunLoop run_loop3;
  store->TakeEarlyFailureReports(
      kOrigin, base::BindOnce(
                   [](base::ListValue* out, base::OnceClosure quit,
                      base::ListValue res) {
                     *out = std::move(res);
                     std::move(quit).Run();
                   },
                   &reports, run_loop3.QuitClosure()));
  run_loop3.Run();

  EXPECT_GE(reports.size(), 13u);
  EXPECT_LE(reports.size(), 15u);

  // Verify FIFO eviction order: older entries (lower index) should be deleted
  // first. The remaining entries should have consecutive indexes ending at 19.
  int expected_start_index = 20 - reports.size();
  for (size_t i = 0; i < reports.size(); ++i) {
    const base::DictValue* dict = reports[i].GetIfDict();
    ASSERT_TRUE(dict);
    std::optional<int> idx = dict->FindInt("index");
    ASSERT_TRUE(idx);
    EXPECT_EQ(*idx, static_cast<int>(expected_start_index + i));
  }
}

TEST_F(DeclarativePerformanceObserverStoreTest, ClearData) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStore();

  base::RunLoop run_loop1;
  store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  base::DictValue sample;
  sample.Set("entryType", "navigation");

  base::RunLoop run_loop2;
  store->StoreEarlyFailureReport(kOrigin, sample.Clone(),
                                 run_loop2.QuitClosure());
  run_loop2.Run();

  base::RunLoop run_loop3;
  store->ClearAllData(run_loop3.QuitClosure());
  run_loop3.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));

  base::ListValue reports;
  base::RunLoop run_loop4;
  store->TakeEarlyFailureReports(
      kOrigin, base::BindOnce(
                   [](base::ListValue* out, base::OnceClosure quit,
                      base::ListValue res) {
                     *out = std::move(res);
                     std::move(quit).Run();
                   },
                   &reports, run_loop4.QuitClosure()));
  run_loop4.Run();
  EXPECT_TRUE(reports.empty());
}

TEST_F(DeclarativePerformanceObserverStoreTest, ClearSelectiveData) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  const url::Origin kOtherOrigin =
      url::Origin::Create(GURL("https://other.com/"));
  auto store = CreateStore();

  {
    base::RunLoop run_loop1;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop1.QuitClosure());
    run_loop1.Run();
  }

  {
    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOtherOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();
  }

  base::DictValue sample;
  sample.Set("entryType", "navigation");

  {
    base::RunLoop run_loop_store1;
    store->StoreEarlyFailureReport(kOrigin, sample.Clone(),
                                   run_loop_store1.QuitClosure());
    run_loop_store1.Run();
  }

  {
    base::RunLoop run_loop_store2;
    store->StoreEarlyFailureReport(kOtherOrigin, sample.Clone(),
                                   run_loop_store2.QuitClosure());
    run_loop_store2.Run();
  }

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));
  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOtherOrigin));

  {
    base::RunLoop run_loop3;
    store->ClearDataForOrigin(kOrigin, run_loop3.QuitClosure());
    run_loop3.Run();
  }

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOtherOrigin));

  // kOrigin's reports should be deleted:
  {
    base::ListValue reports;
    base::RunLoop run_loop_take1;
    store->TakeEarlyFailureReports(
        kOrigin, base::BindOnce(
                     [](base::ListValue* out, base::OnceClosure quit,
                        base::ListValue res) {
                       *out = std::move(res);
                       std::move(quit).Run();
                     },
                     &reports, run_loop_take1.QuitClosure()));
    run_loop_take1.Run();
    EXPECT_TRUE(reports.empty());
  }

  // kOtherOrigin's reports should still be present:
  {
    base::ListValue reports;
    base::RunLoop run_loop_take2;
    store->TakeEarlyFailureReports(
        kOtherOrigin, base::BindOnce(
                          [](base::ListValue* out, base::OnceClosure quit,
                             base::ListValue res) {
                            *out = std::move(res);
                            std::move(quit).Run();
                          },
                          &reports, run_loop_take2.QuitClosure()));
    run_loop_take2.Run();
    EXPECT_EQ(reports.size(), 1u);
  }
}

TEST_F(DeclarativePerformanceObserverStoreTest,
       RejectsReportExceedingQuotaLimit) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  auto store = CreateStoreInMemory();
  base::HistogramTester histogram_tester;

  // Set quota to 100 bytes for testing.
  base::RunLoop run_loop_quota;
  store->SetQuotaLimitForTesting(100, run_loop_quota.QuitClosure());
  run_loop_quota.Run();

  // Create an oversized report (150 bytes).
  base::DictValue oversized_report;
  oversized_report.Set("data", std::string(150, 'a'));

  base::RunLoop run_loop_store;
  store->StoreEarlyFailureReport(kOrigin, oversized_report.Clone(),
                                 run_loop_store.QuitClosure());
  run_loop_store.Run();

  // Verify that the size violation UMA is recorded (kReportTooLarge = 4).
  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.StoreReportResult",
      /*sample=*/4, /*expected_bucket_count=*/1);

  // The oversized report should be rejected.
  base::ListValue reports;
  base::RunLoop run_loop_take;
  store->TakeEarlyFailureReports(
      kOrigin, base::BindOnce(
                   [](base::ListValue* out, base::OnceClosure quit,
                      base::ListValue res) {
                     *out = std::move(res);
                     std::move(quit).Run();
                   },
                   &reports, run_loop_take.QuitClosure()));
  run_loop_take.Run();
  EXPECT_TRUE(reports.empty());
}

TEST_F(DeclarativePerformanceObserverStoreTest, ClearDataForOriginDuringLoad) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("TestProfile3");

  // 1. Populate database with a policy.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // 2. Start a new store but call ClearDataForOrigin before loading finishes.
  auto store = std::make_unique<DeclarativePerformanceObserverStore>(
      /*is_in_memory=*/false, profile_path, nullptr, base::DoNothing());

  base::RunLoop run_loop_clear;
  store->ClearDataForOrigin(kOrigin, run_loop_clear.QuitClosure());
  run_loop_clear.Run();

  // The policy should be cleared and NOT resurrected when the load completes.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, ClearAllDataDuringLoad) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("TestProfile4");

  // 1. Populate database.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop3;
    store->Close(run_loop3.QuitClosure());
    run_loop3.Run();
  }

  // 2. Start new store and call ClearAllData before load finishes.
  auto store = std::make_unique<DeclarativePerformanceObserverStore>(
      /*is_in_memory=*/false, profile_path, nullptr, base::DoNothing());

  base::RunLoop run_loop_clear;
  store->ClearAllData(run_loop_clear.QuitClosure());
  run_loop_clear.Run();

  // All policies should be cleared.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, ClearDataWithFilterDuringLoad) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  const url::Origin kOtherOrigin =
      url::Origin::Create(GURL("https://other.com/"));
  base::FilePath profile_path = temp_dir_.GetPath().AppendASCII("TestProfile5");

  // 1. Populate database with both policies.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOrigin, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop3;
    store->SetEarlyFailurePolicy(kOtherOrigin, true, run_loop3.QuitClosure());
    run_loop3.Run();

    base::RunLoop run_loop4;
    store->Close(run_loop4.QuitClosure());
    run_loop4.Run();
  }

  // 2. Start a new store but call ClearDataWithFilter before loading finishes.
  auto store = std::make_unique<DeclarativePerformanceObserverStore>(
      /*is_in_memory=*/false, profile_path, nullptr, base::DoNothing());

  auto filter = base::BindRepeating(
      [](const url::Origin& target, const url::Origin& origin) {
        return origin == target;
      },
      kOrigin);

  base::RunLoop run_loop_clear;
  store->ClearDataWithFilter(std::move(filter), run_loop_clear.QuitClosure());
  run_loop_clear.Run();

  // kOrigin should be cleared, but kOtherOrigin should remain present and NOT
  // be affected by the load completion.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOtherOrigin));
}

TEST_F(DeclarativePerformanceObserverStoreTest, Enforces7DayTTL) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath db_path =
      temp_dir_.GetPath().AppendASCII("declarative_performance_observer.db");

  // 1. Initialize store once to create the database schema, then close it.
  {
    auto store = CreateStore();
    base::RunLoop run_loop;
    store->Close(run_loop.QuitClosure());
    run_loop.Run();
  }

  // 2. Manually populate DB with both expired (8 days old) and active (1 day
  // old) reports. We do this by creating a transient SQL connection to bypass
  // the store's automatic timestamping.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path));

    int64_t eight_days_ago_us = (base::Time::Now() - base::Days(8))
                                    .ToDeltaSinceWindowsEpoch()
                                    .InMicroseconds();
    int64_t one_day_ago_us = (base::Time::Now() - base::Days(1))
                                 .ToDeltaSinceWindowsEpoch()
                                 .InMicroseconds();

    sql::Statement insert_stmt(db.GetUniqueStatement(
        "INSERT INTO declarative_performance_observer_reports "
        "(origin, payload, created_at) VALUES (?, ?, ?)"));

    // Insert expired report (8 days old)
    insert_stmt.BindString(0, kOrigin.Serialize());
    insert_stmt.BindString(1, "{\"test_key\":\"expired\"}");
    insert_stmt.BindInt64(2, eight_days_ago_us);
    ASSERT_TRUE(insert_stmt.Run());

    insert_stmt.Reset(true);

    // Insert active report (1 day old)
    insert_stmt.BindString(0, kOrigin.Serialize());
    insert_stmt.BindString(1, "{\"test_key\":\"active\"}");
    insert_stmt.BindInt64(2, one_day_ago_us);
    ASSERT_TRUE(insert_stmt.Run());
  }

  // 2. Instantiate the store. Its initialization sequence should trigger the
  // 7-day TTL cleanup.
  auto store = CreateStore();

  // 3. Take reports and verify that ONLY the active (1 day old) report remains.
  base::ListValue reports;
  base::RunLoop run_loop;
  store->TakeEarlyFailureReports(
      kOrigin, base::BindOnce(
                   [](base::ListValue* out, base::OnceClosure quit,
                      base::ListValue res) {
                     *out = std::move(res);
                     std::move(quit).Run();
                   },
                   &reports, run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_EQ(reports.size(), 1u);
  const base::DictValue* dict = reports[0].GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ(*(dict->FindString("test_key")), "active");
}

TEST_F(DeclarativePerformanceObserverStoreTest, RecordsStorageStatsHistograms) {
  base::FilePath profile_path =
      temp_dir_.GetPath().AppendASCII("TestProfileStats");

  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://example1.com/"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://example2.com/"));

  // 1. Populate the database first with some data.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    // Set policies
    base::RunLoop run_loop_p1;
    store->SetEarlyFailurePolicy(kOrigin1, true, run_loop_p1.QuitClosure());
    run_loop_p1.Run();

    base::RunLoop run_loop_p2;
    store->SetEarlyFailurePolicy(kOrigin2, true, run_loop_p2.QuitClosure());
    run_loop_p2.Run();

    // Add reports
    base::DictValue report;
    report.Set("test", "data");

    base::RunLoop run_loop_r1;
    store->StoreEarlyFailureReport(kOrigin1, report.Clone(),
                                   run_loop_r1.QuitClosure());
    run_loop_r1.Run();

    base::RunLoop run_loop_r2;
    store->StoreEarlyFailureReport(kOrigin2, report.Clone(),
                                   run_loop_r2.QuitClosure());
    run_loop_r2.Run();

    base::RunLoop run_loop_close;
    store->Close(run_loop_close.QuitClosure());
    run_loop_close.Run();
  }

  // 2. Re-create the store. During initialization it should log database stats
  // histograms.
  base::HistogramTester histogram_tester;
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();
  }

  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.StoredOriginCount",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.StoredReportCount",
      /*sample=*/2, /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Storage.DeclarativePerformanceObserver.DatabaseSize", 1);
}

TEST_F(DeclarativePerformanceObserverStoreTest, RecordsEvictionHistograms) {
  base::HistogramTester histogram_tester;
  auto store = CreateStoreInMemory();

  // Set quota to 100 bytes for testing.
  base::RunLoop run_loop_quota;
  store->SetQuotaLimitForTesting(100, run_loop_quota.QuitClosure());
  run_loop_quota.Run();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));

  // Store two small reports (~40 bytes each).
  base::DictValue report1;
  report1.Set("k", std::string(30, 'a'));  // total payload size is ~40 bytes
  base::RunLoop run_loop_r1;
  store->StoreEarlyFailureReport(kOrigin, report1.Clone(),
                                 run_loop_r1.QuitClosure());
  run_loop_r1.Run();

  base::DictValue report2;
  report2.Set("k", std::string(30, 'b'));
  base::RunLoop run_loop_r2;
  store->StoreEarlyFailureReport(kOrigin, report2.Clone(),
                                 run_loop_r2.QuitClosure());
  run_loop_r2.Run();

  // No eviction should have occurred yet.
  histogram_tester.ExpectTotalCount(
      "Storage.DeclarativePerformanceObserver.EvictionCount", 0);

  // Store a third report that pushes total size above 100 bytes.
  base::DictValue report3;
  report3.Set("k", std::string(30, 'c'));
  base::RunLoop run_loop_r3;
  store->StoreEarlyFailureReport(kOrigin, report3.Clone(),
                                 run_loop_r3.QuitClosure());
  run_loop_r3.Run();

  // Eviction must have triggered, deleting 1 report.
  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.EvictionCount",
      /*sample=*/1, /*expected_bucket_count=*/1);
}

TEST_F(DeclarativePerformanceObserverStoreTest,
       RecordsExpiredReportsHistogram) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::FilePath profile_path =
      temp_dir_.GetPath().AppendASCII("TestProfileTTLExpired");
  base::FilePath db_file = profile_path.Append(
      FILE_PATH_LITERAL("declarative_performance_observer.db"));

  base::Time now = base::Time::Now();
  int64_t eight_days_ago_us =
      (now - base::Days(8)).ToDeltaSinceWindowsEpoch().InMicroseconds();

  // 1. Initialize store once to create the database schema, then close it.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop_close;
    store->Close(run_loop_close.QuitClosure());
    run_loop_close.Run();
  }

  // 2. Manually populate DB with an expired report.
  {
    sql::Database db(sql::Database::Tag("DeclarativePerformanceObserver"));
    ASSERT_TRUE(base::CreateDirectory(db_file.DirName()));
    ASSERT_TRUE(db.Open(db_file));

    sql::Statement insert_stmt(db.GetUniqueStatement(
        "INSERT INTO declarative_performance_observer_reports (origin, "
        "payload, created_at) VALUES (?, ?, ?)"));
    insert_stmt.BindString(0, kOrigin.Serialize());
    insert_stmt.BindString(1, "{\"test\":\"expired\"}");
    insert_stmt.BindInt64(2, eight_days_ago_us);
    ASSERT_TRUE(insert_stmt.Run());
  }

  // 2. Initialize the store. It should trigger TTL cleanup and log expired
  // count.
  base::HistogramTester histogram_tester;
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();
  }

  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.ExpiredReportsCount",
      /*sample=*/1, /*expected_bucket_count=*/1);
}

TEST_F(DeclarativePerformanceObserverStoreTest,
       RecordsReadReportResultHistogram) {
  base::FilePath profile_path =
      temp_dir_.GetPath().AppendASCII("TestProfileReadResult");
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));

  // 1. Populate database manually with a corrupted JSON payload.
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop_close;
    store->Close(run_loop_close.QuitClosure());
    run_loop_close.Run();
  }

  {
    sql::Database db(sql::Database::Tag("DeclarativePerformanceObserver"));
    ASSERT_TRUE(db.Open(profile_path.Append(
        FILE_PATH_LITERAL("declarative_performance_observer.db"))));
    sql::Statement insert_stmt(db.GetUniqueStatement(
        "INSERT INTO declarative_performance_observer_reports (origin, "
        "payload, created_at) VALUES (?, ?, ?)"));
    insert_stmt.BindString(0, kOrigin.Serialize());
    insert_stmt.BindString(1, "invalid_json_payload");
    insert_stmt.BindInt64(
        2, base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
    ASSERT_TRUE(insert_stmt.Run());
  }

  // 2. Read reports from the store and check UMA.
  base::HistogramTester histogram_tester;
  {
    base::RunLoop run_loop;
    auto store = std::make_unique<DeclarativePerformanceObserverStore>(
        /*is_in_memory=*/false, profile_path, nullptr, run_loop.QuitClosure());
    run_loop.Run();

    base::RunLoop run_loop_take;
    store->TakeEarlyFailureReports(
        kOrigin,
        base::BindOnce([](base::OnceClosure quit,
                          base::ListValue res) { std::move(quit).Run(); },
                       run_loop_take.QuitClosure()));
    run_loop_take.Run();
  }

  // We expect kFailedJsonParse = 3
  histogram_tester.ExpectUniqueSample(
      "Storage.DeclarativePerformanceObserver.ReadReportResult",
      /*sample=*/3, /*expected_bucket_count=*/1);
}

TEST_F(DeclarativePerformanceObserverStoreTest, HandlesVersion1AndRazes) {
  base::FilePath db_path =
      temp_dir_.GetPath().AppendASCII("declarative_performance_observer.db");

  // Create a v1 version of the database.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));

    // Create the old version table (without created_at).
    ASSERT_TRUE(
        db.Execute("CREATE TABLE declarative_performance_observer_policies ("
                   "origin TEXT PRIMARY KEY NOT NULL, "
                   "capture_early_failures BOOLEAN NOT NULL)"));
    ASSERT_TRUE(db.Execute(
        "INSERT INTO declarative_performance_observer_policies "
        "(origin, capture_early_failures) VALUES ('https://old.com', 1)"));
  }

  auto store = CreateStore();

  // Set a new policy which forces the DB to initialize to version 2 and raze
  // the old.
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));
  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(
      kOrigin, true,
      base::BindOnce([](base::OnceClosure quit) { std::move(quit).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  base::RunLoop run_loop_close;
  store->Close(run_loop_close.QuitClosure());
  run_loop_close.Run();
  store.reset();

  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 2, 2));
    EXPECT_EQ(2, meta_table.GetVersionNumber());

    // Verify that the V1 data ('https://old.com') was razed and is no longer
    // readable.
    sql::Statement statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM "
                              "declarative_performance_observer_policies "
                              "WHERE origin = 'https://old.com'"));
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(0, statement.ColumnInt(0));

    // Verify that the newly inserted V2 policy ('https://example.com') exists.
    sql::Statement new_statement(
        db.GetUniqueStatement("SELECT COUNT(*) FROM "
                              "declarative_performance_observer_policies "
                              "WHERE origin = 'https://example.com'"));
    ASSERT_TRUE(new_statement.Step());
    EXPECT_EQ(1, new_statement.ColumnInt(0));
  }
}

TEST_F(DeclarativePerformanceObserverStoreTest, EnforcesPolicyTTL) {
  base::FilePath db_path =
      temp_dir_.GetPath().AppendASCII("declarative_performance_observer.db");

  const url::Origin kOriginExpired =
      url::Origin::Create(GURL("https://expired.com/"));
  const url::Origin kOriginValid =
      url::Origin::Create(GURL("https://valid.com/"));

  // 1. Create a store and insert two policies.
  {
    auto store = CreateStore();
    base::RunLoop run_loop1;
    store->SetEarlyFailurePolicy(kOriginExpired, true, run_loop1.QuitClosure());
    run_loop1.Run();

    base::RunLoop run_loop2;
    store->SetEarlyFailurePolicy(kOriginValid, true, run_loop2.QuitClosure());
    run_loop2.Run();

    base::RunLoop run_loop_close;
    store->Close(run_loop_close.QuitClosure());
    run_loop_close.Run();
  }

  // 2. Manually alter the creation timestamp of the first policy to 8 days ago
  // in the DB.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path));
    int64_t expired_time = (base::Time::Now() - base::Days(8))
                               .ToDeltaSinceWindowsEpoch()
                               .InMicroseconds();
    sql::Statement statement(db.GetUniqueStatement(
        "UPDATE declarative_performance_observer_policies SET created_at = ? "
        "WHERE origin = ?"));
    statement.BindInt64(0, expired_time);
    statement.BindString(1, kOriginExpired.Serialize());
    ASSERT_TRUE(statement.Run());
  }

  // 3. Re-open the store. The expired policy should be cleaned up.
  auto store = CreateStore();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOriginExpired));
  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOriginValid));
}

TEST_F(DeclarativePerformanceObserverStoreTest, EnforcesPolicyTTLInMemory) {
  auto store = CreateStore();
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com/"));

  // 1. Set the policy. It should be active immediately.
  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(
      kOrigin, true,
      base::BindOnce([](base::OnceClosure quit) { std::move(quit).Run(); },
                     run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(store->HasEarlyFailurePolicy(kOrigin));

  // 2. Fast forward time by 8 days.
  task_environment_.FastForwardBy(base::Days(8));

  // 3. Check again. The cache lookup should detect the expiration, remove it
  // from the memory cache, and return false.
  EXPECT_FALSE(store->HasEarlyFailurePolicy(kOrigin));
}

}  // namespace content
