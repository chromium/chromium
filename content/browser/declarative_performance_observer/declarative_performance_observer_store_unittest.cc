// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class DeclarativePerformanceObserverStoreTest : public testing::Test {
 public:
  DeclarativePerformanceObserverStoreTest() = default;
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

  // 1. Manually populate DB with both expired (8 days old) and active (1 day
  // old) reports. We do this by creating a transient SQL connection to bypass
  // the store's automatic timestamping.
  {
    sql::Database db(sql::test::kTestTag);
    ASSERT_TRUE(db.Open(db_path));

    // Re-create tables manually if they don't exist yet (just in case)
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE IF NOT EXISTS declarative_performance_observer_reports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "origin TEXT NOT NULL, "
        "payload BLOB NOT NULL, "
        "created_at INTEGER NOT NULL)"));

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
    insert_stmt.BindBlob(
        1, base::as_byte_span(std::string_view("{\"test_key\":\"expired\"}")));
    insert_stmt.BindInt64(2, eight_days_ago_us);
    ASSERT_TRUE(insert_stmt.Run());

    insert_stmt.Reset(true);

    // Insert active report (1 day old)
    insert_stmt.BindString(0, kOrigin.Serialize());
    insert_stmt.BindBlob(
        1, base::as_byte_span(std::string_view("{\"test_key\":\"active\"}")));
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

}  // namespace content
