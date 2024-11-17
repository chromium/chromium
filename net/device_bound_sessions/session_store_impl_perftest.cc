// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store_impl.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"
#include "net/base/schemeful_site.h"
#include "net/base/test_completion_callback.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

const base::FilePath::CharType dbsc_filename[] =
    FILE_PATH_LITERAL("DBSC_Sessions");

static constexpr char kMetricPrefixDbscSS[] = "DBSCSessionStore.";
static constexpr char kMetricOperationDurationMs[] = "operation_duration";
static const int kNumSites = 200;
static const int kSessionsPerSite = 5;

constexpr crypto::SignatureVerifier::SignatureAlgorithm
    kAcceptableAlgorithms[] = {crypto::SignatureVerifier::ECDSA_SHA256};
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kUserBlocking;

perf_test::PerfResultReporter SetUpDbscSSReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixDbscSS, story);
  reporter.RegisterImportantMetric(kMetricOperationDurationMs, "ms");
  return reporter;
}

}  // namespace

class DBSCSessionStorePerfTest : public testing::Test {
 public:
  DBSCSessionStorePerfTest() : key_service_(task_manager_) {}

  void CreateStore() {
    store_ = std::make_unique<SessionStoreImpl>(
        temp_dir_.GetPath().Append(dbsc_filename), key_service_);
  }

  void DeleteStore() {
    base::RunLoop run_loop;
    store_->SetShutdownCallbackForTesting(run_loop.QuitClosure());
    store_ = nullptr;
    run_loop.Run();
  }

  SessionStore::SessionsMap LoadSessions() {
    base::RunLoop run_loop;
    SessionStore::SessionsMap loaded_sessions;
    store_->LoadSessions(base::BindLambdaForTesting(
        [&run_loop, &loaded_sessions](SessionStore::SessionsMap sessions) {
          loaded_sessions = std::move(sessions);
          run_loop.Quit();
        }));
    run_loop.Run();
    return loaded_sessions;
  }

  unexportable_keys::UnexportableKeyId GenerateNewKey() {
    base::test::TestFuture<
        unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>>
        generate_future;
    key_service_.GenerateSigningKeySlowlyAsync(
        kAcceptableAlgorithms, kTaskPriority, generate_future.GetCallback());
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        key_id = generate_future.Get();
    CHECK(key_id.has_value());
    return *key_id;
  }

  void AddSession(int site_idx, int session_idx) {
    std::string session_str = base::StringPrintf("session_id_%d", session_idx);
    std::string url_str =
        base::StringPrintf("https://%d.example%d.test", session_idx, site_idx);
    std::string refresh_url =
        base::StringPrintf("https://example%d.test/refresh.html", site_idx);
    std::string cookie_name =
        base::StringPrintf("cookie_%d_%d", site_idx, session_idx);
    std::string cookie_attr =
        base::StringPrintf("Secure; Domain=example%d.test", site_idx);

    SessionParams::Scope scope;
    std::vector<SessionParams::Credential> cookie_credentials(
        {SessionParams::Credential{cookie_name, cookie_attr}});
    SessionParams params{session_str, refresh_url, std::move(scope),
                         std::move(cookie_credentials)};
    std::unique_ptr<Session> session =
        Session::CreateIfValid(params, GURL(url_str));
    ASSERT_TRUE(session);

    session->set_unexportable_key_id(GenerateNewKey());

    store_->SaveSession(SchemefulSite(GURL(url_str)), *session);
  }

  unsigned int NumSessionsInStore() { return store_->GetAllSessions().size(); }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    CreateStore();
    LoadSessions();  // empty load
    ASSERT_TRUE(store_);
    // StartPerfMeasurement();
    for (int site_num = 0; site_num < kNumSites; site_num++) {
      for (int session_num = 0; session_num < kSessionsPerSite; ++session_num) {
        AddSession(site_num, session_num);
      }
    }

    // Delete the store. This action will cause all the session data to be
    // written to disk.
    DeleteStore();
  }

  void TearDown() override {
    if (store_) {
      DeleteStore();
    }
  }

  void StartPerfMeasurement() {
    DCHECK(perf_measurement_start_.is_null());
    perf_measurement_start_ = base::Time::Now();
  }

  void EndPerfMeasurement(const std::string& story) {
    DCHECK(!perf_measurement_start_.is_null());
    base::TimeDelta elapsed = base::Time::Now() - perf_measurement_start_;
    perf_measurement_start_ = base::Time();
    auto reporter = SetUpDbscSSReporter(story);
    reporter.AddResult(kMetricOperationDurationMs, elapsed.InMillisecondsF());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SessionStoreImpl> store_;
  crypto::ScopedMockUnexportableKeyProvider scoped_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl key_service_;
  base::Time perf_measurement_start_;
};

// Test the performance of load
// TODO(crbug.com/371964293): Refactor this test to use the
// Google Benchmark library instead.
TEST_F(DBSCSessionStorePerfTest, TestLoadPerformance) {
  CreateStore();
  VLOG(0) << "Beginning load from disk..";
  StartPerfMeasurement();
  SessionStore::SessionsMap sessions_map = LoadSessions();
  EndPerfMeasurement("load");
  EXPECT_EQ(NumSessionsInStore(), (unsigned int)(kNumSites * kSessionsPerSite));
  VLOG(0) << "Loaded " << sessions_map.size() << " sessions.";
}

}  // namespace net::device_bound_sessions
