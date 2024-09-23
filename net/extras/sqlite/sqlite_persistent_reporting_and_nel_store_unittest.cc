// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const base::FilePath::CharType kReportingAndNELStoreFilename[] =
    FILE_PATH_LITERAL("ReportingAndNEL");

const IPAddress kServerIP = IPAddress(192, 168, 0, 1);
const std::string kHeader = "{\"report_to\":\"group\",\"max_age\":86400}";
const std::string kHeaderMaxAge0 = "{\"report_to\":\"group\",\"max_age\":0}";
const std::string kGroupName1 = "group1";
const std::string kGroupName2 = "group2";
const base::Time kExpires = base::Time::Now() + base::Days(7);

enum class Op { kAdd, kDelete, kUpdate, kUpdateDetails };

struct TestCase {
  std::vector<Op> operations;
  size_t expected_queue_length;
};

// Testcases for coalescing of pending operations. In each case, the given
// sequence of operations should be coalesced down to |expected_queue_length|
// actual operations queued.
const std::vector<TestCase> kCoalescingTestcases = {
    {{Op::kAdd, Op::kDelete}, 1u},
    {{Op::kUpdate, Op::kDelete}, 1u},
    {{Op::kAdd, Op::kUpdate, Op::kDelete}, 1u},
    {{Op::kUpdate, Op::kUpdate}, 1u},
    {{Op::kAdd, Op::kUpdate, Op::kUpdate}, 2u},
    {{Op::kDelete, Op::kAdd}, 2u},
    {{Op::kDelete, Op::kAdd, Op::kUpdate}, 3u},
    {{Op::kDelete, Op::kAdd, Op::kUpdate, Op::kUpdate}, 3u},
    {{Op::kDelete, Op::kDelete}, 1u},
    {{Op::kDelete, Op::kAdd, Op::kDelete}, 1u},
    {{Op::kDelete, Op::kAdd, Op::kUpdate, Op::kDelete}, 1u}};

// This is for Reporting endpoint groups, which have both UPDATE_DETAILS and
// UPDATE_ACCESS_TIME. These additional testcases test that UPDATE_DETAILS
// overwrites UPDATE_ACCESS_TIME, but not vice versa.
const std::vector<TestCase> kCoalescingTestcasesForUpdateDetails = {
    {{Op::kUpdateDetails, Op::kDelete}, 1u},
    {{Op::kAdd, Op::kUpdateDetails, Op::kDelete}, 1u},
    {{Op::kUpdateDetails, Op::kUpdateDetails}, 1u},
    {{Op::kUpdate, Op::kUpdateDetails}, 1u},
    {{Op::kUpdateDetails, Op::kUpdate}, 2u},
    {{Op::kAdd, Op::kUpdateDetails, Op::kUpdate}, 3u},
    {{Op::kAdd, Op::kUpdateDetails, Op::kUpdate, Op::kUpdateDetails}, 2u},
    {{Op::kDelete, Op::kAdd, Op::kUpdateDetails}, 3u},
    {{Op::kDelete, Op::kAdd, Op::kUpdateDetails, Op::kUpdateDetails}, 3u},
    {{Op::kDelete, Op::kAdd, Op::kUpdate, Op::kUpdateDetails}, 3u},
    {{Op::kDelete, Op::kAdd, Op::kUpdateDetails, Op::kUpdate}, 4u}};

}  // namespace

class SQLitePersistentReportingAndNelStoreTest
    : public TestWithTaskEnvironment {
 public:
  SQLitePersistentReportingAndNelStoreTest() {
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
  }

  void CreateStore() {
    store_ = std::make_unique<SQLitePersistentReportingAndNelStore>(
        temp_dir_.GetPath().Append(kReportingAndNELStoreFilename),
        client_task_runner_, background_task_runner_);
  }

  void DestroyStore() {
    store_.reset();
    // Make sure we wait until the destructor has run by running all
    // TaskEnvironment tasks.
    RunUntilIdle();
  }

  // Call this on a brand new database that should have nothing stored in it.
  void InitializeStore() {
    std::vector<NetworkErrorLoggingService::NelPolicy> nel_policies;
    LoadNelPolicies(&nel_policies);
    EXPECT_EQ(0u, nel_policies.size());

    // One load should be sufficient to initialize the database, but we might as
    // well load everything to check that there is nothing in the database.
    std::vector<ReportingEndpoint> endpoints;
    std::vector<CachedReportingEndpointGroup> groups;
    LoadReportingClients(&endpoints, &groups);
    EXPECT_EQ(0u, endpoints.size());
    EXPECT_EQ(0u, groups.size());
  }

  void LoadNelPolicies(
      std::vector<NetworkErrorLoggingService::NelPolicy>* policies_out) {
    base::RunLoop run_loop;
    store_->LoadNelPolicies(base::BindOnce(
        &SQLitePersistentReportingAndNelStoreTest::OnNelPoliciesLoaded,
        base::Unretained(this), &run_loop, policies_out));
    run_loop.Run();
  }

  void OnNelPoliciesLoaded(
      base::RunLoop* run_loop,
      std::vector<NetworkErrorLoggingService::NelPolicy>* policies_out,
      std::vector<NetworkErrorLoggingService::NelPolicy> policies) {
    policies_out->swap(policies);
    run_loop->Quit();
  }

  void LoadReportingClients(
      std::vector<ReportingEndpoint>* endpoints_out,
      std::vector<CachedReportingEndpointGroup>* groups_out) {
    base::RunLoop run_loop;
    store_->LoadReportingClients(base::BindOnce(
        &SQLitePersistentReportingAndNelStoreTest::OnReportingClientsLoaded,
        base::Unretained(this), &run_loop, endpoints_out, groups_out));
    run_loop.Run();
  }

  void OnReportingClientsLoaded(
      base::RunLoop* run_loop,
      std::vector<ReportingEndpoint>* endpoints_out,
      std::vector<CachedReportingEndpointGroup>* groups_out,
      std::vector<ReportingEndpoint> endpoints,
      std::vector<CachedReportingEndpointGroup> groups) {
    endpoints_out->swap(endpoints);
    groups_out->swap(groups);
    run_loop->Quit();
  }

  std::string ReadRawDBContents() {
    std::string contents;
    if (!base::ReadFileToString(
            temp_dir_.GetPath().Append(kReportingAndNELStoreFilename),
            &contents)) {
      return std::string();
    }
    return contents;
  }

  bool WithinOneMicrosecond(base::Time t1, base::Time t2) {
    base::TimeDelta delta = t1 - t2;
    return delta.magnitude() < base::Microseconds(1);
  }

  void WaitOnEvent(base::WaitableEvent* event) {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    event->Wait();
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { DestroyStore(); }

  NetworkErrorLoggingService::NelPolicy MakeNelPolicy(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      base::Time last_used) {
    NetworkErrorLoggingService::NelPolicy policy;
    policy.key = NetworkErrorLoggingService::NelPolicyKey(
        network_anonymization_key, origin);
    policy.received_ip_address = IPAddress::IPv4Localhost();
    policy.report_to = "group";
    policy.expires = kExpires;
    policy.success_fraction = 0.0;
    policy.failure_fraction = 1.0;
    policy.include_subdomains = false;
    policy.last_used = last_used;
    return policy;
  }

  ReportingEndpoint MakeReportingEndpoint(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const std::string& group_name,
      const GURL& url,
      int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority,
      int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight) {
    ReportingEndpoint::EndpointInfo info;
    info.url = url;
    info.priority = priority;
    info.weight = weight;
    ReportingEndpoint endpoint(
        ReportingEndpointGroupKey(network_anonymization_key, origin, group_name,
                                  ReportingTargetType::kDeveloper),
        std::move(info));
    return endpoint;
  }

  CachedReportingEndpointGroup MakeReportingEndpointGroup(
      const NetworkAnonymizationKey& network_anonymization_key,
      const url::Origin& origin,
      const std::string& group_name,
      base::Time last_used,
      OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT,
      base::Time expires = kExpires) {
    return CachedReportingEndpointGroup(
        ReportingEndpointGroupKey(network_anonymization_key, origin, group_name,
                                  ReportingTargetType::kDeveloper),
        include_subdomains, expires, last_used);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  // Use origins distinct from those used in origin fields of keys, to avoid any
  // risk of tests passing due to comparing origins that are the same but come
  // from different sources.
  const NetworkAnonymizationKey kNak1_ =
      NetworkAnonymizationKey::CreateCrossSite(
          SchemefulSite(GURL("https://top-frame-origin-nak1.test")));
  const NetworkAnonymizationKey kNak2_ =
      NetworkAnonymizationKey::CreateCrossSite(
          SchemefulSite(GURL("https://top-frame-origin-nak2.test")));

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SQLitePersistentReportingAndNelStore> store_;
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

TEST_F(SQLitePersistentReportingAndNelStoreTest, CreateDBAndTables) {
  CreateStore();
  InitializeStore();
  EXPECT_NE(nullptr, store_.get());
  std::string contents = ReadRawDBContents();
  EXPECT_NE("", contents);
  EXPECT_NE(std::string::npos, contents.find("nel_policies"));
  EXPECT_NE(std::string::npos, contents.find("reporting_endpoints"));
  EXPECT_NE(std::string::npos, contents.find("reporting_endpoint_groups"));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, TestInvalidMetaTableRecovery) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy1 = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);
  store_->AddNelPolicy(policy1);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  // Load the stored policy.
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy1.key, policies[0].key);
  EXPECT_EQ(policy1.received_ip_address, policies[0].received_ip_address);
  EXPECT_EQ(policy1.report_to, policies[0].report_to);
  EXPECT_TRUE(WithinOneMicrosecond(policy1.expires, policies[0].expires));
  EXPECT_EQ(policy1.include_subdomains, policies[0].include_subdomains);
  EXPECT_EQ(policy1.success_fraction, policies[0].success_fraction);
  EXPECT_EQ(policy1.failure_fraction, policies[0].failure_fraction);
  EXPECT_TRUE(WithinOneMicrosecond(policy1.last_used, policies[0].last_used));
  DestroyStore();
  policies.clear();

  // Now corrupt the meta table.
  {
    sql::Database db;
    ASSERT_TRUE(
        db.Open(temp_dir_.GetPath().Append(kReportingAndNELStoreFilename)));
    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&db, 1, 1));
    ASSERT_TRUE(db.Execute("DELETE FROM meta"));
    db.Close();
  }

  base::HistogramTester hist_tester;

  // Upon loading, the database should be reset to a good, blank state.
  CreateStore();
  LoadNelPolicies(&policies);
  ASSERT_EQ(0U, policies.size());

  hist_tester.ExpectUniqueSample("ReportingAndNEL.CorruptMetaTableRecovered",
                                 true, 1);

  // Verify that, after, recovery, the database persists properly.
  NetworkErrorLoggingService::NelPolicy policy2 = MakeNelPolicy(
      kNak2_, url::Origin::Create(GURL("https://www.bar.test")), now);
  store_->AddNelPolicy(policy2);
  DestroyStore();

  CreateStore();
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy2.key, policies[0].key);
  EXPECT_EQ(policy2.received_ip_address, policies[0].received_ip_address);
  EXPECT_EQ(policy2.report_to, policies[0].report_to);
  EXPECT_TRUE(WithinOneMicrosecond(policy2.expires, policies[0].expires));
  EXPECT_EQ(policy2.include_subdomains, policies[0].include_subdomains);
  EXPECT_EQ(policy2.success_fraction, policies[0].success_fraction);
  EXPECT_EQ(policy2.failure_fraction, policies[0].failure_fraction);
  EXPECT_TRUE(WithinOneMicrosecond(policy2.last_used, policies[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, PersistNelPolicy) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);
  store_->AddNelPolicy(policy);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  // Load the stored policy.
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy.key, policies[0].key);
  EXPECT_EQ(policy.received_ip_address, policies[0].received_ip_address);
  EXPECT_EQ(policy.report_to, policies[0].report_to);
  EXPECT_TRUE(WithinOneMicrosecond(policy.expires, policies[0].expires));
  EXPECT_EQ(policy.include_subdomains, policies[0].include_subdomains);
  EXPECT_EQ(policy.success_fraction, policies[0].success_fraction);
  EXPECT_EQ(policy.failure_fraction, policies[0].failure_fraction);
  EXPECT_TRUE(WithinOneMicrosecond(policy.last_used, policies[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, LoadFailed) {
  // Inject a db initialization failure by creating a directory where the db
  // file should be.
  ASSERT_TRUE(base::CreateDirectory(
      temp_dir_.GetPath().Append(kReportingAndNELStoreFilename)));
  store_ = std::make_unique<SQLitePersistentReportingAndNelStore>(
      temp_dir_.GetPath().Append(kReportingAndNELStoreFilename),
      client_task_runner_, background_task_runner_);

  // InitializeStore() checks that we receive empty vectors of NEL policies,
  // reporting endpoints, and reporting endpoint groups, signifying the failure
  // to load.
  InitializeStore();
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, UpdateNelPolicyAccessTime) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);
  store_->AddNelPolicy(policy);

  policy.last_used = now + base::Days(1);
  store_->UpdateNelPolicyAccessTime(policy);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  // Load the stored policy.
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy.key, policies[0].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy.last_used, policies[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, DeleteNelPolicy) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy1 = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);
  NetworkErrorLoggingService::NelPolicy policy2 = MakeNelPolicy(
      kNak2_, url::Origin::Create(GURL("https://www.bar.test")), now);
  store_->AddNelPolicy(policy1);
  store_->AddNelPolicy(policy2);

  store_->DeleteNelPolicy(policy1);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  // |policy1| is no longer in the database but |policy2| remains.
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy2.key, policies[0].key);

  // Delete after having closed and reopened.
  store_->DeleteNelPolicy(policy2);
  DestroyStore();
  CreateStore();

  // |policy2| is also gone.
  policies.clear();
  LoadNelPolicies(&policies);
  EXPECT_EQ(0u, policies.size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       NelPolicyUniquenessConstraint) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://www.bar.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://www.foo.test"));

  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  base::Time later = now + base::Days(1);

  // Add 3 entries, 2 identical except for NAK, 2 identical except for origin.
  // Entries should not conflict with each other. These are added in lexical
  // order.
  NetworkErrorLoggingService::NelPolicy policy1 =
      MakeNelPolicy(kNak1_, kOrigin1, now);
  NetworkErrorLoggingService::NelPolicy policy2 =
      MakeNelPolicy(kNak1_, kOrigin2, now);
  NetworkErrorLoggingService::NelPolicy policy3 =
      MakeNelPolicy(kNak2_, kOrigin1, now);
  store_->AddNelPolicy(policy1);
  store_->AddNelPolicy(policy2);
  store_->AddNelPolicy(policy3);

  // Add policies that are identical except for expiration time. These should
  // trigger a warning an fail to execute.
  NetworkErrorLoggingService::NelPolicy policy4 =
      MakeNelPolicy(kNak1_, kOrigin1, later);
  NetworkErrorLoggingService::NelPolicy policy5 =
      MakeNelPolicy(kNak1_, kOrigin2, later);
  NetworkErrorLoggingService::NelPolicy policy6 =
      MakeNelPolicy(kNak2_, kOrigin1, later);
  store_->AddNelPolicy(policy4);
  store_->AddNelPolicy(policy5);
  store_->AddNelPolicy(policy6);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);

  // Only the first 3 policies should be in the store.

  ASSERT_EQ(3u, policies.size());

  EXPECT_EQ(policy1.key, policies[0].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy1.last_used, policies[0].last_used));

  EXPECT_EQ(policy2.key, policies[1].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy2.last_used, policies[1].last_used));

  EXPECT_EQ(policy3.key, policies[2].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy3.last_used, policies[2].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, CoalesceNelPolicyOperations) {
  NetworkErrorLoggingService::NelPolicy policy =
      MakeNelPolicy(kNak1_, url::Origin::Create(GURL("https://www.foo.test")),
                    base::Time::Now());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  for (const TestCase& testcase : kCoalescingTestcases) {
    CreateStore();
    base::RunLoop run_loop;
    store_->LoadNelPolicies(base::BindLambdaForTesting(
        [&](std::vector<NetworkErrorLoggingService::NelPolicy>) {
          run_loop.Quit();
        }));
    run_loop.Run();

    // Wedge the background thread to make sure it doesn't start consuming the
    // queue.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                       base::Unretained(this), &event));

    // Now run the ops, and check how much gets queued.
    for (const Op op : testcase.operations) {
      switch (op) {
        case Op::kAdd:
          store_->AddNelPolicy(policy);
          break;

        case Op::kDelete:
          store_->DeleteNelPolicy(policy);
          break;

        case Op::kUpdate:
          store_->UpdateNelPolicyAccessTime(policy);
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    EXPECT_EQ(testcase.expected_queue_length,
              store_->GetQueueLengthForTesting());

    event.Signal();
    RunUntilIdle();
  }
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       DontCoalesceUnrelatedNelPolicies) {
  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy1 = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);
  // Only has different host.
  NetworkErrorLoggingService::NelPolicy policy2 = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.bar.test")), now);
  // Only has different NetworkAnonymizationKey.
  NetworkErrorLoggingService::NelPolicy policy3 = MakeNelPolicy(
      kNak2_, url::Origin::Create(GURL("https://www.foo.test")), now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  // Delete on |policy2| and |policy3| should not cancel addition of unrelated
  // |policy1|.
  store_->AddNelPolicy(policy1);
  store_->DeleteNelPolicy(policy2);
  store_->DeleteNelPolicy(policy3);
  EXPECT_EQ(3u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       DontPersistNelPoliciesWithTransientNetworkAnonymizationKeys) {
  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  NetworkErrorLoggingService::NelPolicy policy =
      MakeNelPolicy(NetworkAnonymizationKey::CreateTransient(),
                    url::Origin::Create(GURL("https://www.foo.test")), now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddNelPolicy(policy);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->UpdateNelPolicyAccessTime(policy);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->DeleteNelPolicy(policy);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  EXPECT_EQ(0u, policies.size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       NelPoliciesRestoredWithNetworkAnonymizationKeysDisabled) {
  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  // Policy with non-empty NetworkAnonymizationKey.
  NetworkErrorLoggingService::NelPolicy policy = MakeNelPolicy(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddNelPolicy(policy);
  EXPECT_EQ(1u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close the database, disable kPartitionConnectionsByNetworkIsolationKey,
  // and re-open it.
  DestroyStore();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  CreateStore();
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);

  // No entries should be restored.
  ASSERT_EQ(0u, policies.size());

  // Now reload the store with kPartitionConnectionsByNetworkIsolationKey
  // enabled again.
  DestroyStore();
  feature_list.Reset();
  CreateStore();
  LoadNelPolicies(&policies);

  // The entry is back!
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy.key, policies[0].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy.expires, policies[0].expires));
}

// These tests test that a SQLitePersistentReportingAndNelStore
// can be used by a NetworkErrorLoggingService to persist NEL policies.
class SQLitePersistNelTest : public SQLitePersistentReportingAndNelStoreTest {
 public:
  SQLitePersistNelTest() = default;

  void SetUp() override {
    SQLitePersistentReportingAndNelStoreTest::SetUp();
    SetUpNetworkErrorLoggingService();
  }

  void TearDown() override {
    service_->OnShutdown();
    service_.reset();
    reporting_service_.reset();
    SQLitePersistentReportingAndNelStoreTest::TearDown();
  }

  void SetUpNetworkErrorLoggingService() {
    CreateStore();
    service_ = NetworkErrorLoggingService::Create(store_.get());
    reporting_service_ = std::make_unique<TestReportingService>();
    service_->SetReportingService(reporting_service_.get());
    service_->SetClockForTesting(&clock_);
  }

  void SimulateRestart() {
    TearDown();
    SetUpNetworkErrorLoggingService();
  }

  NetworkErrorLoggingService::RequestDetails MakeRequestDetails(
      const NetworkAnonymizationKey& network_anonymization_key,
      const GURL& url,
      Error error_type) {
    NetworkErrorLoggingService::RequestDetails details;

    details.network_anonymization_key = network_anonymization_key;
    details.uri = url;
    details.referrer = GURL("https://referrer.com/");
    details.user_agent = "Mozilla/1.0";
    details.server_ip = kServerIP;
    details.method = "GET";
    details.status_code = 0;
    details.elapsed_time = base::Seconds(1);
    details.type = error_type;
    details.reporting_upload_depth = 0;

    return details;
  }

 protected:
  base::SimpleTestClock clock_;
  std::unique_ptr<NetworkErrorLoggingService> service_;
  std::unique_ptr<TestReportingService> reporting_service_;
};

TEST_F(SQLitePersistNelTest, AddAndRetrieveNelPolicy) {
  const GURL kUrl("https://www.foo.test");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const NetworkErrorLoggingService::NelPolicyKey kKey(kNak1_, kOrigin);

  service_->OnHeader(kNak1_, kOrigin, kServerIP, kHeader);
  RunUntilIdle();

  EXPECT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey));
  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  EXPECT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey));

  EXPECT_THAT(reporting_service_->reports(),
              testing::ElementsAre(ReportUrlIs(kUrl)));
}

TEST_F(SQLitePersistNelTest, AddAndDeleteNelPolicy) {
  const GURL kUrl("https://www.foo.test");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const NetworkErrorLoggingService::NelPolicyKey kKey(kNak1_, kOrigin);

  service_->OnHeader(kNak1_, kOrigin, kServerIP, kHeader);
  RunUntilIdle();

  EXPECT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey));
  SimulateRestart();

  // Deletes the stored policy.
  service_->OnHeader(kNak1_, kOrigin, kServerIP, kHeaderMaxAge0);
  RunUntilIdle();

  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey));
  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey));
  EXPECT_EQ(0u, reporting_service_->reports().size());
}

TEST_F(SQLitePersistNelTest, ExpirationTimeIsPersisted) {
  const GURL kUrl("https://www.foo.test");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  const NetworkAnonymizationKey kNak;

  service_->OnHeader(kNak, kOrigin, kServerIP, kHeader);
  RunUntilIdle();

  // Makes the policy we just added expired.
  clock_.Advance(base::Seconds(86401));

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak, kUrl, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  EXPECT_EQ(0u, reporting_service_->reports().size());

  // Add the policy again so that it is not expired.
  service_->OnHeader(kNak, kOrigin, kServerIP, kHeader);

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak, kUrl, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  EXPECT_THAT(reporting_service_->reports(),
              testing::ElementsAre(ReportUrlIs(kUrl)));
}

TEST_F(SQLitePersistNelTest, OnRequestUpdatesAccessTime) {
  const GURL kUrl("https://www.foo.test");
  const url::Origin kOrigin = url::Origin::Create(kUrl);

  service_->OnHeader(kNak1_, kOrigin, kServerIP, kHeader);
  RunUntilIdle();

  SimulateRestart();

  // Update the access time by sending a request.
  clock_.Advance(base::Seconds(100));
  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  EXPECT_THAT(reporting_service_->reports(),
              testing::ElementsAre(ReportUrlIs(kUrl)));

  SimulateRestart();
  // Check that the policy's access time has been updated.
  base::Time now = clock_.Now();
  NetworkErrorLoggingService::NelPolicy policy =
      MakeNelPolicy(kNak1_, kOrigin, now);
  std::vector<NetworkErrorLoggingService::NelPolicy> policies;
  LoadNelPolicies(&policies);
  ASSERT_EQ(1u, policies.size());
  EXPECT_EQ(policy.key, policies[0].key);
  EXPECT_TRUE(WithinOneMicrosecond(policy.last_used, policies[0].last_used));
}

TEST_F(SQLitePersistNelTest, RemoveSomeBrowsingData) {
  const GURL kUrl1("https://www.foo.test");
  const url::Origin kOrigin1 = url::Origin::Create(kUrl1);
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://www.bar.test"));
  const NetworkErrorLoggingService::NelPolicyKey kKey1(kNak1_, kOrigin1);
  const NetworkErrorLoggingService::NelPolicyKey kKey2(kNak2_, kOrigin2);

  service_->OnHeader(kNak1_, kOrigin1, kServerIP, kHeader);
  service_->OnHeader(kNak2_, kOrigin2, kServerIP, kHeader);
  RunUntilIdle();

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl1, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  ASSERT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey1));
  ASSERT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey2));
  EXPECT_THAT(reporting_service_->reports(),
              testing::ElementsAre(ReportUrlIs(kUrl1)));

  SimulateRestart();

  service_->RemoveBrowsingData(base::BindRepeating(
      [](const std::string& host, const url::Origin& origin) {
        return origin.host() == host;
      },
      kOrigin1.host()));
  RunUntilIdle();

  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey1));
  EXPECT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey2));

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl1, ERR_INVALID_RESPONSE));
  RunUntilIdle();
  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey1));
  EXPECT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey2));
  EXPECT_EQ(0u, reporting_service_->reports().size());
}

TEST_F(SQLitePersistNelTest, RemoveAllBrowsingData) {
  const GURL kUrl1("https://www.foo.test");
  const url::Origin kOrigin1 = url::Origin::Create(kUrl1);
  const GURL kUrl2("https://www.bar.test");
  const url::Origin kOrigin2 = url::Origin::Create(kUrl2);
  const NetworkErrorLoggingService::NelPolicyKey kKey1(kNak1_, kOrigin1);
  const NetworkErrorLoggingService::NelPolicyKey kKey2(kNak2_, kOrigin2);

  service_->OnHeader(kNak1_, kOrigin1, kServerIP, kHeader);
  service_->OnHeader(kNak2_, kOrigin2, kServerIP, kHeader);
  RunUntilIdle();

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl1, ERR_INVALID_RESPONSE));
  service_->OnRequest(MakeRequestDetails(kNak2_, kUrl2, ERR_INVALID_RESPONSE));
  RunUntilIdle();

  ASSERT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey1));
  ASSERT_EQ(1u, service_->GetPolicyKeysForTesting().count(kKey2));
  EXPECT_THAT(reporting_service_->reports(),
              testing::ElementsAre(ReportUrlIs(kUrl1), ReportUrlIs(kUrl2)));

  SimulateRestart();

  service_->RemoveAllBrowsingData();
  RunUntilIdle();

  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey1));
  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey2));

  SimulateRestart();

  service_->OnRequest(MakeRequestDetails(kNak1_, kUrl1, ERR_INVALID_RESPONSE));
  service_->OnRequest(MakeRequestDetails(kNak2_, kUrl2, ERR_INVALID_RESPONSE));
  RunUntilIdle();
  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey1));
  EXPECT_EQ(0u, service_->GetPolicyKeysForTesting().count(kKey2));
  EXPECT_EQ(0u, reporting_service_->reports().size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, PersistReportingClients) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.test"));

  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  ReportingEndpoint endpoint = MakeReportingEndpoint(
      kNak1_, kOrigin, kGroupName1, GURL("https://endpoint.test/1"));
  CachedReportingEndpointGroup group =
      MakeReportingEndpointGroup(kNak1_, kOrigin, kGroupName1, now);

  store_->AddReportingEndpoint(endpoint);
  store_->AddReportingEndpointGroup(group);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  // Load the stored clients.
  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_EQ(endpoint.group_key.network_anonymization_key,
            endpoints[0].group_key.network_anonymization_key);
  EXPECT_EQ(endpoint.group_key.origin, endpoints[0].group_key.origin);
  EXPECT_EQ(endpoint.group_key.group_name, endpoints[0].group_key.group_name);
  EXPECT_EQ(endpoint.info.url, endpoints[0].info.url);
  EXPECT_EQ(endpoint.info.priority, endpoints[0].info.priority);
  EXPECT_EQ(endpoint.info.weight, endpoints[0].info.weight);
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(group.group_key.network_anonymization_key,
            groups[0].group_key.network_anonymization_key);
  EXPECT_EQ(group.group_key.origin, groups[0].group_key.origin);
  EXPECT_EQ(group.group_key.group_name, groups[0].group_key.group_name);
  EXPECT_EQ(group.include_subdomains, groups[0].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group.expires, groups[0].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group.last_used, groups[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       UpdateReportingEndpointGroupAccessTime) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now);

  store_->AddReportingEndpointGroup(group);

  group.last_used = now + base::Days(1);
  store_->UpdateReportingEndpointGroupAccessTime(group);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(group.group_key.network_anonymization_key,
            groups[0].group_key.network_anonymization_key);
  EXPECT_EQ(group.group_key.origin, groups[0].group_key.origin);
  EXPECT_EQ(group.group_key.group_name, groups[0].group_key.group_name);
  EXPECT_TRUE(WithinOneMicrosecond(group.last_used, groups[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       UpdateReportingEndpointDetails) {
  CreateStore();
  InitializeStore();
  ReportingEndpoint endpoint = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/1"));

  store_->AddReportingEndpoint(endpoint);

  endpoint.info.priority = 10;
  endpoint.info.weight = 10;
  store_->UpdateReportingEndpointDetails(endpoint);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_EQ(endpoint.group_key.network_anonymization_key,
            endpoints[0].group_key.network_anonymization_key);
  EXPECT_EQ(endpoint.group_key.origin, endpoints[0].group_key.origin);
  EXPECT_EQ(endpoint.group_key.group_name, endpoints[0].group_key.group_name);
  EXPECT_EQ(endpoint.info.url, endpoints[0].info.url);
  EXPECT_EQ(endpoint.info.priority, endpoints[0].info.priority);
  EXPECT_EQ(endpoint.info.weight, endpoints[0].info.weight);
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       UpdateReportingEndpointGroupDetails) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now, OriginSubdomains::EXCLUDE, kExpires);

  store_->AddReportingEndpointGroup(group);

  group.last_used = now + base::Days(1);
  group.expires = kExpires + base::Days(1);
  group.include_subdomains = OriginSubdomains::INCLUDE;
  store_->UpdateReportingEndpointGroupDetails(group);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(group.group_key.network_anonymization_key,
            groups[0].group_key.network_anonymization_key);
  EXPECT_EQ(group.group_key.origin, groups[0].group_key.origin);
  EXPECT_EQ(group.group_key.group_name, groups[0].group_key.group_name);
  EXPECT_EQ(group.include_subdomains, groups[0].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group.expires, groups[0].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group.last_used, groups[0].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, DeleteReportingEndpoint) {
  CreateStore();
  InitializeStore();
  ReportingEndpoint endpoint1 = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/1"));
  ReportingEndpoint endpoint2 = MakeReportingEndpoint(
      kNak2_, url::Origin::Create(GURL("https://www.bar.test")), kGroupName2,
      GURL("https://endpoint.test/2"));

  store_->AddReportingEndpoint(endpoint1);
  store_->AddReportingEndpoint(endpoint2);

  store_->DeleteReportingEndpoint(endpoint1);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_EQ(endpoint2.info.url, endpoints[0].info.url);

  store_->DeleteReportingEndpoint(endpoint2);
  DestroyStore();
  CreateStore();

  endpoints.clear();
  LoadReportingClients(&endpoints, &groups);
  EXPECT_EQ(0u, endpoints.size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest, DeleteReportingEndpointGroup) {
  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group1 = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now);
  CachedReportingEndpointGroup group2 = MakeReportingEndpointGroup(
      kNak2_, url::Origin::Create(GURL("https://www.bar.test")), kGroupName2,
      now);

  store_->AddReportingEndpointGroup(group1);
  store_->AddReportingEndpointGroup(group2);

  store_->DeleteReportingEndpointGroup(group1);

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(group2.group_key.group_name, groups[0].group_key.group_name);

  store_->DeleteReportingEndpointGroup(group2);
  DestroyStore();
  CreateStore();

  groups.clear();
  LoadReportingClients(&endpoints, &groups);
  EXPECT_EQ(0u, groups.size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       ReportingEndpointUniquenessConstraint) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://www.bar.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://www.foo.test"));
  const GURL kEndpoint("https://endpoint.test/1");

  CreateStore();
  InitializeStore();

  // Add 3 entries, 2 identical except for NAK, 2 identical except for origin.
  // Entries should not conflict with each other. These are added in lexical
  // order.
  ReportingEndpoint endpoint1 =
      MakeReportingEndpoint(kNak1_, kOrigin1, kGroupName1, kEndpoint,
                            1 /* priority */, 1 /* weight */);
  ReportingEndpoint endpoint2 =
      MakeReportingEndpoint(kNak1_, kOrigin2, kGroupName1, kEndpoint,
                            2 /* priority */, 2 /* weight */);
  ReportingEndpoint endpoint3 =
      MakeReportingEndpoint(kNak2_, kOrigin2, kGroupName1, kEndpoint,
                            3 /* priority */, 3 /* weight */);
  store_->AddReportingEndpoint(endpoint1);
  store_->AddReportingEndpoint(endpoint2);
  store_->AddReportingEndpoint(endpoint3);

  // Add entries that are identical except for expiration time. These should
  // trigger a warning an fail to execute.
  ReportingEndpoint endpoint4 =
      MakeReportingEndpoint(kNak1_, kOrigin1, kGroupName1, kEndpoint,
                            4 /* priority */, 4 /* weight */);
  ReportingEndpoint endpoint5 =
      MakeReportingEndpoint(kNak1_, kOrigin2, kGroupName1, kEndpoint,
                            5 /* priority */, 5 /* weight */);
  ReportingEndpoint endpoint6 =
      MakeReportingEndpoint(kNak2_, kOrigin2, kGroupName1, kEndpoint,
                            6 /* priority */, 6 /* weight */);
  store_->AddReportingEndpoint(endpoint4);
  store_->AddReportingEndpoint(endpoint5);
  store_->AddReportingEndpoint(endpoint6);

  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);

  // Only the first 3 endpoints should be in the store.

  ASSERT_EQ(3u, endpoints.size());

  EXPECT_EQ(endpoint1.group_key, endpoints[0].group_key);
  EXPECT_EQ(endpoint1.info.url, endpoints[0].info.url);
  EXPECT_EQ(endpoint1.info.priority, endpoints[0].info.priority);
  EXPECT_EQ(endpoint1.info.weight, endpoints[0].info.weight);

  EXPECT_EQ(endpoint2.group_key, endpoints[1].group_key);
  EXPECT_EQ(endpoint2.info.url, endpoints[1].info.url);
  EXPECT_EQ(endpoint2.info.priority, endpoints[1].info.priority);
  EXPECT_EQ(endpoint2.info.weight, endpoints[1].info.weight);

  EXPECT_EQ(endpoint3.group_key, endpoints[2].group_key);
  EXPECT_EQ(endpoint3.info.url, endpoints[2].info.url);
  EXPECT_EQ(endpoint3.info.priority, endpoints[2].info.priority);
  EXPECT_EQ(endpoint3.info.weight, endpoints[2].info.weight);
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       ReportingEndpointGroupUniquenessConstraint) {
  const url::Origin kOrigin1 =
      url::Origin::Create(GURL("https://www.bar.test"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("https://www.foo.test"));

  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  base::Time later = now + base::Days(7);

  // Add 3 entries, 2 identical except for NAK, 2 identical except for origin.
  // Entries should not conflict with each other. These are added in lexical
  // order.
  CachedReportingEndpointGroup group1 =
      MakeReportingEndpointGroup(kNak1_, kOrigin1, kGroupName1, now);
  CachedReportingEndpointGroup group2 =
      MakeReportingEndpointGroup(kNak1_, kOrigin2, kGroupName1, now);
  CachedReportingEndpointGroup group3 =
      MakeReportingEndpointGroup(kNak2_, kOrigin1, kGroupName1, now);
  store_->AddReportingEndpointGroup(group1);
  store_->AddReportingEndpointGroup(group2);
  store_->AddReportingEndpointGroup(group3);

  // Add entries that are identical except for expiration time. These should
  // trigger a warning an fail to execute.
  CachedReportingEndpointGroup group4 =
      MakeReportingEndpointGroup(kNak1_, kOrigin1, kGroupName1, later);
  CachedReportingEndpointGroup group5 =
      MakeReportingEndpointGroup(kNak1_, kOrigin2, kGroupName1, later);
  CachedReportingEndpointGroup group6 =
      MakeReportingEndpointGroup(kNak2_, kOrigin1, kGroupName1, later);
  store_->AddReportingEndpointGroup(group4);
  store_->AddReportingEndpointGroup(group5);
  store_->AddReportingEndpointGroup(group6);

  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);

  // Only the first 3 endpoints should be in the store.

  ASSERT_EQ(3u, groups.size());

  EXPECT_EQ(group1.group_key, groups[0].group_key);
  EXPECT_EQ(group1.include_subdomains, groups[0].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group1.expires, groups[0].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group1.last_used, groups[0].last_used));

  EXPECT_EQ(group2.group_key, groups[1].group_key);
  EXPECT_EQ(group2.include_subdomains, groups[1].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group2.expires, groups[1].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group2.last_used, groups[1].last_used));

  EXPECT_EQ(group3.group_key, groups[2].group_key);
  EXPECT_EQ(group3.include_subdomains, groups[2].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group3.expires, groups[2].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group3.last_used, groups[2].last_used));
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       CoalesceReportingEndpointOperations) {
  ReportingEndpoint endpoint = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/1"));

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  for (const TestCase& testcase : kCoalescingTestcases) {
    CreateStore();
    base::RunLoop run_loop;
    store_->LoadReportingClients(base::BindLambdaForTesting(
        [&](std::vector<ReportingEndpoint>,
            std::vector<CachedReportingEndpointGroup>) { run_loop.Quit(); }));
    run_loop.Run();

    // Wedge the background thread to make sure it doesn't start consuming the
    // queue.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                       base::Unretained(this), &event));

    // Now run the ops, and check how much gets queued.
    for (const Op op : testcase.operations) {
      switch (op) {
        case Op::kAdd:
          store_->AddReportingEndpoint(endpoint);
          break;

        case Op::kDelete:
          store_->DeleteReportingEndpoint(endpoint);
          break;

        case Op::kUpdate:
          // Endpoints only have UPDATE_DETAILS, so in this case we use kUpdate
          // for that.
          store_->UpdateReportingEndpointDetails(endpoint);
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    EXPECT_EQ(testcase.expected_queue_length,
              store_->GetQueueLengthForTesting());

    event.Signal();
    RunUntilIdle();
  }
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       DontCoalesceUnrelatedReportingEndpoints) {
  CreateStore();
  InitializeStore();

  ReportingEndpoint endpoint1 = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/1"));
  // Only has different host.
  ReportingEndpoint endpoint2 = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.bar.test")), kGroupName1,
      GURL("https://endpoint.test/2"));
  // Only has different NetworkAnonymizationKey.
  ReportingEndpoint endpoint3 = MakeReportingEndpoint(
      kNak2_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/3"));

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  // Delete on |endpoint2| and |endpoint3| should not cancel addition of
  // unrelated |endpoint1|.
  store_->AddReportingEndpoint(endpoint1);
  store_->DeleteReportingEndpoint(endpoint2);
  store_->DeleteReportingEndpoint(endpoint3);
  EXPECT_EQ(3u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       CoalesceReportingEndpointGroupOperations) {
  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  for (const TestCase& testcase : kCoalescingTestcases) {
    CreateStore();
    base::RunLoop run_loop;
    store_->LoadReportingClients(base::BindLambdaForTesting(
        [&](std::vector<ReportingEndpoint>,
            std::vector<CachedReportingEndpointGroup>) { run_loop.Quit(); }));
    run_loop.Run();

    // Wedge the background thread to make sure it doesn't start consuming the
    // queue.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                       base::Unretained(this), &event));

    // Now run the ops, and check how much gets queued.
    for (const Op op : testcase.operations) {
      switch (op) {
        case Op::kAdd:
          store_->AddReportingEndpointGroup(group);
          break;

        case Op::kDelete:
          store_->DeleteReportingEndpointGroup(group);
          break;

        case Op::kUpdate:
          store_->UpdateReportingEndpointGroupAccessTime(group);
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }

    EXPECT_EQ(testcase.expected_queue_length,
              store_->GetQueueLengthForTesting());

    event.Signal();
    RunUntilIdle();
  }

  // Additional test cases for UPDATE_DETAILS.
  for (const TestCase& testcase : kCoalescingTestcasesForUpdateDetails) {
    CreateStore();
    base::RunLoop run_loop;
    store_->LoadReportingClients(base::BindLambdaForTesting(
        [&](std::vector<ReportingEndpoint>,
            std::vector<CachedReportingEndpointGroup>) { run_loop.Quit(); }));
    run_loop.Run();

    // Wedge the background thread to make sure it doesn't start consuming the
    // queue.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                       base::Unretained(this), &event));

    // Now run the ops, and check how much gets queued.
    for (const Op op : testcase.operations) {
      switch (op) {
        case Op::kAdd:
          store_->AddReportingEndpointGroup(group);
          break;

        case Op::kDelete:
          store_->DeleteReportingEndpointGroup(group);
          break;

        case Op::kUpdate:
          store_->UpdateReportingEndpointGroupAccessTime(group);
          break;

        case Op::kUpdateDetails:
          store_->UpdateReportingEndpointGroupDetails(group);
          break;
      }
    }

    EXPECT_EQ(testcase.expected_queue_length,
              store_->GetQueueLengthForTesting());

    event.Signal();
    RunUntilIdle();
  }
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       DontCoalesceUnrelatedReportingEndpointGroups) {
  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group1 = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now);
  // Only has different host.
  CachedReportingEndpointGroup group2 = MakeReportingEndpointGroup(
      kNak1_, url::Origin::Create(GURL("https://www.bar.test")), kGroupName1,
      now);
  // Only has different NetworkAnonymizationKey.
  CachedReportingEndpointGroup group3 = MakeReportingEndpointGroup(
      kNak2_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  // Delete on |group2| and |group3| should not cancel addition of unrelated
  // |group1|.
  store_->AddReportingEndpointGroup(group1);
  store_->DeleteReportingEndpointGroup(group2);
  store_->DeleteReportingEndpointGroup(group3);
  EXPECT_EQ(3u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       DontPersistReportingEndpointsWithTransientNetworkAnonymizationKeys) {
  CreateStore();
  InitializeStore();

  ReportingEndpoint endpoint =
      MakeReportingEndpoint(NetworkAnonymizationKey::CreateTransient(),
                            url::Origin::Create(GURL("https://www.foo.test")),
                            kGroupName1, GURL("https://endpoint.test/1"));

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddReportingEndpoint(endpoint);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->UpdateReportingEndpointDetails(endpoint);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->DeleteReportingEndpoint(endpoint);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(0u, endpoints.size());
}

TEST_F(
    SQLitePersistentReportingAndNelStoreTest,
    DontPersistReportingEndpointGroupsWithTransientNetworkAnonymizationKeys) {
  CreateStore();
  InitializeStore();

  base::Time now = base::Time::Now();
  CachedReportingEndpointGroup group = MakeReportingEndpointGroup(
      NetworkAnonymizationKey::CreateTransient(),
      url::Origin::Create(GURL("https://www.foo.test")), kGroupName1, now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddReportingEndpointGroup(group);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->UpdateReportingEndpointGroupAccessTime(group);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->UpdateReportingEndpointGroupDetails(group);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());
  store_->DeleteReportingEndpointGroup(group);
  EXPECT_EQ(0u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close and reopen the database.
  DestroyStore();
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  ASSERT_EQ(0u, groups.size());
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       ReportingEndpointsRestoredWithNetworkAnonymizationKeysDisabled) {
  CreateStore();
  InitializeStore();

  // Endpoint with non-empty NetworkAnonymizationKey.
  ReportingEndpoint endpoint = MakeReportingEndpoint(
      kNak1_, url::Origin::Create(GURL("https://www.foo.test")), kGroupName1,
      GURL("https://endpoint.test/"));

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddReportingEndpoint(endpoint);
  EXPECT_EQ(1u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close the database, disable kPartitionConnectionsByNetworkIsolationKey,
  // and re-open it.
  DestroyStore();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  LoadReportingClients(&endpoints, &groups);
  // No entries should be restored.
  ASSERT_EQ(0u, endpoints.size());

  // Now reload the store with kPartitionConnectionsByNetworkIsolationKey
  // enabled again.
  DestroyStore();
  feature_list.Reset();
  CreateStore();
  LoadReportingClients(&endpoints, &groups);

  // The entry is back!
  ASSERT_EQ(1u, endpoints.size());
  EXPECT_EQ(endpoint.group_key, endpoints[0].group_key);
  EXPECT_EQ(endpoint.info.url, endpoints[0].info.url);
  EXPECT_EQ(endpoint.info.priority, endpoints[0].info.priority);
  EXPECT_EQ(endpoint.info.weight, endpoints[0].info.weight);
}

TEST_F(SQLitePersistentReportingAndNelStoreTest,
       ReportingEndpointGroupsRestoredWithNetworkAnonymizationKeysDisabled) {
  CreateStore();
  InitializeStore();

  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.test"));

  CreateStore();
  InitializeStore();
  base::Time now = base::Time::Now();
  // Group with non-empty NetworkAnonymizationKey.
  CachedReportingEndpointGroup group =
      MakeReportingEndpointGroup(kNak1_, kOrigin, kGroupName1, now);

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Wedge the background thread to make sure it doesn't start consuming the
  // queue.
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SQLitePersistentReportingAndNelStoreTest::WaitOnEvent,
                     base::Unretained(this), &event));

  store_->AddReportingEndpointGroup(group);
  EXPECT_EQ(1u, store_->GetQueueLengthForTesting());

  event.Signal();
  RunUntilIdle();

  // Close the database, disable kPartitionConnectionsByNetworkIsolationKey,
  // and re-open it.
  DestroyStore();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);
  CreateStore();

  std::vector<ReportingEndpoint> endpoints;
  std::vector<CachedReportingEndpointGroup> groups;
  // No entries should be restored.
  LoadReportingClients(&endpoints, &groups);
  EXPECT_TRUE(groups.empty());

  // Now reload the store with kPartitionConnectionsByNetworkIsolationKey
  // enabled again.
  DestroyStore();
  feature_list.Reset();
  CreateStore();
  LoadReportingClients(&endpoints, &groups);

  // The entry is back!
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ(group.group_key, groups[0].group_key);
  EXPECT_EQ(group.include_subdomains, groups[0].include_subdomains);
  EXPECT_TRUE(WithinOneMicrosecond(group.expires, groups[0].expires));
  EXPECT_TRUE(WithinOneMicrosecond(group.last_used, groups[0].last_used));
}

}  // namespace net
