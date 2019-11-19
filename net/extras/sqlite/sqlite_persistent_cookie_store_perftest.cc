// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/timer/elapsed_timer.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/log/net_log_with_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "url/gurl.h"

namespace net {

namespace {

const base::FilePath::CharType cookie_filename[] = FILE_PATH_LITERAL("Cookies");

static const int kNumDomains = 300;
static const int kCookiesPerDomain = 50;

// Prime number noticeably larger than kNumDomains or kCookiesPerDomain
// so that multiplying this number by an incrementing index and moduloing
// with those values will return semi-random results.
static const int kRandomSeed = 13093;
static_assert(kRandomSeed > 10 * kNumDomains,
              "kRandomSeed not high enough for number of domains");
static_assert(kRandomSeed > 10 * kCookiesPerDomain,
              "kRandomSeed not high enough for number of cookies per domain");

static constexpr char kMetricPrefixSQLPCS[] = "SQLitePersistentCookieStore.";
static constexpr char kMetricOperationDurationMs[] = "operation_duration";

perf_test::PerfResultReporter SetUpSQLPCSReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixSQLPCS, story);
  reporter.RegisterImportantMetric(kMetricOperationDurationMs, "ms");
  return reporter;
}

}  // namespace

class SQLitePersistentCookieStorePerfTest : public testing::Test {
 public:
  SQLitePersistentCookieStorePerfTest()
      : seed_multiple_(1),
        test_start_(base::Time::Now()),
        loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        key_loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void OnLoaded(std::vector<std::unique_ptr<CanonicalCookie>> cookies) {
    cookies_.swap(cookies);
    loaded_event_.Signal();
  }

  void OnKeyLoaded(std::vector<std::unique_ptr<CanonicalCookie>> cookies) {
    cookies_.swap(cookies);
    key_loaded_event_.Signal();
  }

  void Load() {
    store_->Load(base::BindOnce(&SQLitePersistentCookieStorePerfTest::OnLoaded,
                                base::Unretained(this)),
                 NetLogWithSource());
    loaded_event_.Wait();
  }

  CanonicalCookie CookieFromIndices(int domain_num, int cookie_num) {
    base::Time t(test_start_ +
                 base::TimeDelta::FromMicroseconds(
                     domain_num * kCookiesPerDomain + cookie_num));
    std::string domain_name(base::StringPrintf(".domain_%d.com", domain_num));
    return CanonicalCookie(base::StringPrintf("Cookie_%d", cookie_num), "1",
                           domain_name, "/", t, t, t, false, false,
                           CookieSameSite::NO_RESTRICTION,
                           COOKIE_PRIORITY_DEFAULT);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    store_ = new SQLitePersistentCookieStore(
        temp_dir_.GetPath().Append(cookie_filename), client_task_runner_,
        background_task_runner_, false, nullptr);
    std::vector<CanonicalCookie*> cookies;
    Load();
    ASSERT_EQ(0u, cookies_.size());
    // Creates kNumDomains*kCookiesPerDomain cookies from kNumDomains eTLD+1s.
    for (int domain_num = 0; domain_num < kNumDomains; domain_num++) {
      for (int cookie_num = 0; cookie_num < kCookiesPerDomain; ++cookie_num) {
        store_->AddCookie(CookieFromIndices(domain_num, cookie_num));
      }
    }
    // Replace the store effectively destroying the current one and forcing it
    // to write its data to disk.
    store_ = nullptr;

    // Flush ThreadPool tasks, causing pending commits to run.
    task_environment_.RunUntilIdle();

    store_ = new SQLitePersistentCookieStore(
        temp_dir_.GetPath().Append(cookie_filename), client_task_runner_,
        background_task_runner_, false, nullptr);
  }

  // Pick a random cookie out of the 15000 in the store and return it.
  // Note that this distribution is intended to be random for purposes of
  // probing, but will be the same each time the test is run for
  // reproducibility of performance.
  CanonicalCookie RandomCookie() {
    int consistent_random_value = ++seed_multiple_ * kRandomSeed;
    int domain = consistent_random_value % kNumDomains;
    int cookie_num = consistent_random_value % kCookiesPerDomain;
    return CookieFromIndices(domain, cookie_num);
  }

  void TearDown() override { store_ = nullptr; }

  void StartPerfMeasurement() {
    DCHECK(perf_measurement_start_.is_null());
    perf_measurement_start_ = base::Time::Now();
  }

  void EndPerfMeasurement(const std::string& story) {
    DCHECK(!perf_measurement_start_.is_null());
    base::TimeDelta elapsed = base::Time::Now() - perf_measurement_start_;
    perf_measurement_start_ = base::Time();
    auto reporter = SetUpSQLPCSReporter(story);
    reporter.AddResult(kMetricOperationDurationMs, elapsed.InMillisecondsF());
  }

 protected:
  int seed_multiple_;
  base::Time test_start_;
  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
  base::WaitableEvent loaded_event_;
  base::WaitableEvent key_loaded_event_;
  std::vector<std::unique_ptr<CanonicalCookie>> cookies_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SQLitePersistentCookieStore> store_;
  base::Time perf_measurement_start_;
};

// Test the performance of priority load of cookies for a specific domain key
TEST_F(SQLitePersistentCookieStorePerfTest, TestLoadForKeyPerformance) {
  ASSERT_LT(3, kNumDomains);
  for (int domain_num = 0; domain_num < 3; ++domain_num) {
    std::string domain_name(base::StringPrintf("domain_%d.com", domain_num));
    StartPerfMeasurement();
    store_->LoadCookiesForKey(
        domain_name,
        base::BindOnce(&SQLitePersistentCookieStorePerfTest::OnKeyLoaded,
                       base::Unretained(this)));
    key_loaded_event_.Wait();
    EndPerfMeasurement("load_for_key");

    ASSERT_EQ(50U, cookies_.size());
  }
}

// Test the performance of load
TEST_F(SQLitePersistentCookieStorePerfTest, TestLoadPerformance) {
  StartPerfMeasurement();
  Load();
  EndPerfMeasurement("load");

  ASSERT_EQ(kNumDomains * kCookiesPerDomain, static_cast<int>(cookies_.size()));
}

// Test deletion performance.
TEST_F(SQLitePersistentCookieStorePerfTest, TestDeletePerformance) {
  const int kNumToDelete = 50;
  const int kNumIterations = 400;

  // Figure out the kNumToDelete cookies.
  std::vector<CanonicalCookie> cookies;
  cookies.reserve(kNumToDelete);
  for (int cookie = 0; cookie < kNumToDelete; ++cookie) {
    cookies.push_back(RandomCookie());
  }
  ASSERT_EQ(static_cast<size_t>(kNumToDelete), cookies.size());

  StartPerfMeasurement();
  for (int i = 0; i < kNumIterations; ++i) {
    // Delete and flush
    for (int cookie = 0; cookie < kNumToDelete; ++cookie) {
      store_->DeleteCookie(cookies[cookie]);
    }
    {
      TestClosure test_closure;
      store_->Flush(test_closure.closure());
      test_closure.WaitForResult();
    }

    // Add and flush
    for (int cookie = 0; cookie < kNumToDelete; ++cookie) {
      store_->AddCookie(cookies[cookie]);
    }

    TestClosure test_closure;
    store_->Flush(test_closure.closure());
    test_closure.WaitForResult();
  }
  EndPerfMeasurement("delete");
}

// Test update performance.
TEST_F(SQLitePersistentCookieStorePerfTest, TestUpdatePerformance) {
  const int kNumToUpdate = 50;
  const int kNumIterations = 400;

  // Figure out the kNumToUpdate cookies.
  std::vector<CanonicalCookie> cookies;
  cookies.reserve(kNumToUpdate);
  for (int cookie = 0; cookie < kNumToUpdate; ++cookie) {
    cookies.push_back(RandomCookie());
  }
  ASSERT_EQ(static_cast<size_t>(kNumToUpdate), cookies.size());

  StartPerfMeasurement();
  for (int i = 0; i < kNumIterations; ++i) {
    // Update and flush
    for (int cookie = 0; cookie < kNumToUpdate; ++cookie) {
      store_->UpdateCookieAccessTime(cookies[cookie]);
    }

    TestClosure test_closure;
    store_->Flush(test_closure.closure());
    test_closure.WaitForResult();
  }
  EndPerfMeasurement("update");
}

}  // namespace net
