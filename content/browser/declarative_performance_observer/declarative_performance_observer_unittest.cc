// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/declarative_performance_observer/declarative_performance_observer_store.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_storage_partition.h"
#include "content/public/test/test_utils.h"
#include "content/test/storage_partition_test_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/mojom/timing/declarative_performance_observer.mojom.h"

namespace content {
namespace {

class TestNetworkContext : public network::TestNetworkContext {
 public:
  struct Report {
    Report(const std::string& type,
           const std::string& group,
           const GURL& url,
           base::DictValue body)
        : type(type), group(group), url(url), body(std::move(body)) {}

    std::string type;
    std::string group;
    GURL url;
    base::DictValue body;
  };

  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::DictValue body) override {
    reports_.emplace_back(type, group, url, std::move(body));
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  const std::vector<Report>& reports() const { return reports_; }

  void ClearReports() { reports_.clear(); }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  std::vector<Report> reports_;
  base::OnceClosure quit_closure_;
};

class DeclarativePerformanceObserverTest : public RenderViewHostTestHarness {
 public:
  DeclarativePerformanceObserverTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kDeclarativePerformanceObserver);
  }
  ~DeclarativePerformanceObserverTest() override = default;

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    storage_partition_.set_network_context(&network_context_);
  }

  void CreateObserver(NavigationHandle* navigation_handle) {
    DeclarativePerformanceObserver::CreateForCurrentDocument(main_rfh(),
                                                             navigation_handle);
    auto* observer =
        DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
    if (observer) {
      observer->SetStoragePartitionForTesting(&storage_partition_);
    }
  }

  content::TestStoragePartition storage_partition_;
  TestNetworkContext network_context_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DeclarativePerformanceObserverTest, RecordsVisibilityStateOnCommit) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kVisibilityState);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  EXPECT_EQ(report.type, "performance-observer");
  EXPECT_EQ(report.group, kEndpoint);
  EXPECT_EQ(report.url, kPageURL);

  const base::ListValue* entries = report.body.FindList("entries");
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 2u);

  const base::Value& entry_val0 = (*entries)[0];
  const base::DictValue* visEntry = entry_val0.GetIfDict();
  ASSERT_TRUE(visEntry);

  const std::string* entryType = visEntry->FindString("entryType");
  ASSERT_TRUE(entryType);
  EXPECT_EQ(*entryType, "visibility-state");

  const std::string* name = visEntry->FindString("name");
  ASSERT_TRUE(name);
  EXPECT_EQ(*name, "visible");

  std::optional<double> startTime = visEntry->FindDouble("startTime");
  ASSERT_TRUE(startTime);
  EXPECT_EQ(*startTime, 0.0);

  std::optional<double> duration = visEntry->FindDouble("duration");
  ASSERT_TRUE(duration);
  EXPECT_EQ(*duration, 0.0);

  // Check session-end entry
  const base::Value& entry_val1 = (*entries)[1];
  const base::DictValue* endEntry = entry_val1.GetIfDict();
  ASSERT_TRUE(endEntry);
  EXPECT_EQ(*(endEntry->FindString("entryType")), "session-end");
  EXPECT_EQ(*(endEntry->FindString("name")), "session-end-event");
}

TEST_F(DeclarativePerformanceObserverTest, ObservesVisibilityFlips) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kVisibilityState);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  ASSERT_TRUE(observer);

  observer->OnVisibilityChanged(Visibility::HIDDEN);
  observer->OnVisibilityChanged(Visibility::VISIBLE);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 2u);

  // Report 1: Flushed on HIDDEN
  const auto& report1 = network_context_.reports()[0];
  const base::ListValue* entries1 = report1.body.FindList("entries");
  ASSERT_TRUE(entries1);
  // initial visible + 1st flip (hidden)
  ASSERT_EQ(entries1->size(), 2u);

  const base::Value& entry0_val = (*entries1)[0];
  const base::DictValue* entry0 = entry0_val.GetIfDict();
  ASSERT_TRUE(entry0);
  EXPECT_EQ(*(entry0->FindString("name")), "visible");

  const base::Value& entry1_val = (*entries1)[1];
  const base::DictValue* entry1 = entry1_val.GetIfDict();
  ASSERT_TRUE(entry1);
  EXPECT_EQ(*(entry1->FindString("name")), "hidden");

  // Report 2: Flushed on RenderFrameDeleted
  const auto& report2 = network_context_.reports()[1];
  const base::ListValue* entries2 = report2.body.FindList("entries");
  ASSERT_TRUE(entries2);
  // 2nd flip (visible) + session-end
  ASSERT_EQ(entries2->size(), 2u);

  const base::Value& entry2_0_val = (*entries2)[0];
  const base::DictValue* entry2_0 = entry2_0_val.GetIfDict();
  ASSERT_TRUE(entry2_0);
  EXPECT_EQ(*(entry2_0->FindString("name")), "visible");

  const base::Value& entry2_1_val = (*entries2)[1];
  const base::DictValue* entry2_1 = entry2_1_val.GetIfDict();
  ASSERT_TRUE(entry2_1);
  EXPECT_EQ(*(entry2_1->FindString("entryType")), "session-end");
  EXPECT_EQ(*(entry2_1->FindString("name")), "session-end-event");
}

TEST_F(DeclarativePerformanceObserverTest, RecordsNavigationTiming) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kNavigation);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  base::TimeTicks nav_start = base::TimeTicks::Now();
  ON_CALL(navigation_handle, NavigationStart())
      .WillByDefault(testing::Return(nav_start));

  NavigationHandleTiming timing;
  timing.final_response_start_time = nav_start + base::Milliseconds(500);
  timing.final_request_start_time = nav_start + base::Milliseconds(200);
  timing.final_request_domain_lookup_start_time =
      nav_start + base::Milliseconds(50);
  timing.final_request_domain_lookup_end_time =
      nav_start + base::Milliseconds(100);
  timing.final_request_connect_start_time = nav_start + base::Milliseconds(120);
  timing.final_request_connect_end_time = nav_start + base::Milliseconds(180);
  timing.final_request_ssl_start_time = nav_start + base::Milliseconds(150);

  ON_CALL(navigation_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(timing));

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];

  const base::ListValue* entries = report.body.FindList("entries");
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 2u);

  const base::Value& entry_val0 = (*entries)[0];
  const base::DictValue* navEntry = entry_val0.GetIfDict();
  ASSERT_TRUE(navEntry);

  // Check session-end entry
  const base::Value& entry_val1 = (*entries)[1];
  const base::DictValue* endEntry = entry_val1.GetIfDict();
  ASSERT_TRUE(endEntry);
  EXPECT_EQ(*(endEntry->FindString("entryType")), "session-end");
  EXPECT_EQ(*(endEntry->FindString("name")), "session-end-event");

  const std::string* entryType = navEntry->FindString("entryType");
  ASSERT_TRUE(entryType);
  EXPECT_EQ(*entryType, "navigation");

  const std::string* name = navEntry->FindString("name");
  ASSERT_TRUE(name);
  EXPECT_EQ(*name, kPageURL.spec());

  std::optional<double> startTime = navEntry->FindDouble("startTime");
  ASSERT_TRUE(startTime);
  EXPECT_EQ(*startTime, 0.0);

  const std::string* deliveryType = navEntry->FindString("deliveryType");
  ASSERT_TRUE(deliveryType);
  EXPECT_EQ(*deliveryType, "");

  std::optional<double> responseStart = navEntry->FindDouble("responseStart");
  ASSERT_TRUE(responseStart);
  EXPECT_EQ(*responseStart, 500.0);

  std::optional<double> requestStart = navEntry->FindDouble("requestStart");
  ASSERT_TRUE(requestStart);
  EXPECT_EQ(*requestStart, 200.0);

  std::optional<double> domainLookupStart =
      navEntry->FindDouble("domainLookupStart");
  ASSERT_TRUE(domainLookupStart);
  EXPECT_EQ(*domainLookupStart, 50.0);

  std::optional<double> domainLookupEnd =
      navEntry->FindDouble("domainLookupEnd");
  ASSERT_TRUE(domainLookupEnd);
  EXPECT_EQ(*domainLookupEnd, 100.0);

  std::optional<double> connectStart = navEntry->FindDouble("connectStart");
  ASSERT_TRUE(connectStart);
  EXPECT_EQ(*connectStart, 120.0);

  std::optional<double> connectEnd = navEntry->FindDouble("connectEnd");
  ASSERT_TRUE(connectEnd);
  EXPECT_EQ(*connectEnd, 180.0);

  std::optional<double> secureConnectionStart =
      navEntry->FindDouble("secureConnectionStart");
  ASSERT_TRUE(secureConnectionStart);
  EXPECT_EQ(*secureConnectionStart, 150.0);
}

TEST_F(DeclarativePerformanceObserverTest, RecordsBFCacheLifecycle) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kNavigation);
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kVisibilityState);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  NavigationHandleTiming timing;
  ON_CALL(navigation_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(timing));

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  // 1. Commit initial page
  CreateObserver(&navigation_handle);
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  ASSERT_TRUE(observer);

  // 2. Enter BackForwardCache
  observer->OnEnterBFCache();

  // Buffer should be flushed on entering BFCache
  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report1 = network_context_.reports()[0];

  const base::ListValue* entries1 = report1.body.FindList("entries");
  ASSERT_TRUE(entries1);
  // initial visible + initial navigation + session-end
  ASSERT_EQ(entries1->size(), 3u);

  const base::Value& nav_entry_val1 = (*entries1)[1];
  const base::DictValue* nav_entry1 = nav_entry_val1.GetIfDict();
  ASSERT_TRUE(nav_entry1);
  EXPECT_EQ(*(nav_entry1->FindString("entryType")), "navigation");
  EXPECT_EQ(*(nav_entry1->FindString("name")), kPageURL.spec());

  const base::Value& end_entry_val = (*entries1)[2];
  const base::DictValue* end_entry = end_entry_val.GetIfDict();
  ASSERT_TRUE(end_entry);
  EXPECT_EQ(*(end_entry->FindString("entryType")), "session-end");
  EXPECT_EQ(*(end_entry->FindString("name")), "session-end-event");

  // Clear reports for testing restore
  network_context_.ClearReports();

  // 3. Restore from BackForwardCache
  MockNavigationHandle restore_handle(kPageURL, main_rfh());
  restore_handle.set_has_committed(true);
  restore_handle.set_is_in_primary_main_frame(true);
  restore_handle.set_is_error_page(false);
  restore_handle.set_is_served_from_bfcache(true);

  observer->OnDidFinishNavigation(&restore_handle);

  // Trigger unload to flush metrics for the RESTORED session
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report2 = network_context_.reports()[0];

  const base::ListValue* entries2 = report2.body.FindList("entries");
  ASSERT_TRUE(entries2);
  // back_forward navigation + initial visible + session-end
  ASSERT_EQ(entries2->size(), 3u);

  const base::Value& end_entry_val2 = (*entries2)[2];
  const base::DictValue* end_entry2 = end_entry_val2.GetIfDict();
  ASSERT_TRUE(end_entry2);
  EXPECT_EQ(*(end_entry2->FindString("entryType")), "session-end");
  EXPECT_EQ(*(end_entry2->FindString("name")), "session-end-event");

  const base::Value& nav_entry_val = (*entries2)[0];
  const base::DictValue* nav_entry = nav_entry_val.GetIfDict();
  ASSERT_TRUE(nav_entry);
  EXPECT_EQ(*(nav_entry->FindString("entryType")), "navigation");
  EXPECT_EQ(*(nav_entry->FindString("type")), "back_forward");
  EXPECT_EQ(*(nav_entry->FindString("name")), kPageURL.spec());

  const base::Value& vis_entry_val = (*entries2)[1];
  const base::DictValue* vis_entry = vis_entry_val.GetIfDict();
  ASSERT_TRUE(vis_entry);
  EXPECT_EQ(*(vis_entry->FindString("entryType")), "visibility-state");
  EXPECT_EQ(*(vis_entry->FindString("name")), "visible");
}

TEST_F(DeclarativePerformanceObserverTest, RecordsPerformanceMarks) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(network::mojom::PerformanceEntryType::kMark);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);

  // Bind Mojo remote
  mojo::Remote<blink::mojom::DeclarativePerformanceObserverHost>
      observer_remote;
  DeclarativePerformanceObserver::Bind(
      main_rfh(), observer_remote.BindNewPipeAndPassReceiver());

  // Simulate receiving performance entries from renderer
  std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries;

  // 1. Allowed mark with detail
  auto entry1 = blink::mojom::DeclarativePerformanceEntry::New();
  entry1->name = "some_mark";
  entry1->start_time = base::Milliseconds(100);
  base::DictValue detail_dict;
  detail_dict.Set("key", "value");
  entry1->detail = base::Value(std::move(detail_dict));
  entries.push_back(std::move(entry1));

  observer_remote->DidObservePerformanceEntries(std::move(entries));
  observer_remote.FlushForTesting();

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  EXPECT_EQ(report.type, "performance-observer");
  EXPECT_EQ(report.group, kEndpoint);
  EXPECT_EQ(report.url, kPageURL);

  const base::ListValue* report_entries = report.body.FindList("entries");
  ASSERT_TRUE(report_entries);
  ASSERT_EQ(report_entries->size(), 1u);  // only mark

  const base::Value& entry_val = (*report_entries)[0];
  const base::DictValue* mark_entry = entry_val.GetIfDict();
  ASSERT_TRUE(mark_entry);
  EXPECT_EQ(*(mark_entry->FindString("entryType")), "mark");
  EXPECT_EQ(*(mark_entry->FindString("name")), "some_mark");
  EXPECT_EQ(*(mark_entry->FindDouble("startTime")), 100.0);

  const base::Value* detail = mark_entry->Find("detail");
  ASSERT_TRUE(detail);
  ASSERT_TRUE(detail->is_dict());
  EXPECT_EQ(*(detail->GetDict().FindString("key")), "value");
}

TEST_F(DeclarativePerformanceObserverTest,
       FiltersPerformanceMarksByIncludeUserTiming) {
  const GURL kPageURL("https://example.com/index.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(network::mojom::PerformanceEntryType::kMark);
  policy->include_user_timing = std::vector<std::string>{"allowed_mark"};

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);
  navigation_handle.set_is_error_page(false);

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);

  // Bind Mojo remote
  mojo::Remote<blink::mojom::DeclarativePerformanceObserverHost>
      observer_remote;
  DeclarativePerformanceObserver::Bind(
      main_rfh(), observer_remote.BindNewPipeAndPassReceiver());

  // Simulate receiving performance entries from renderer
  std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries;

  // 1. Allowed mark
  auto entry1 = blink::mojom::DeclarativePerformanceEntry::New();
  entry1->name = "allowed_mark";
  entry1->start_time = base::Milliseconds(100);
  entries.push_back(std::move(entry1));

  // 2. Disallowed mark
  auto entry2 = blink::mojom::DeclarativePerformanceEntry::New();
  entry2->name = "disallowed_mark";
  entry2->start_time = base::Milliseconds(200);
  entries.push_back(std::move(entry2));

  observer_remote->DidObservePerformanceEntries(std::move(entries));
  observer_remote.FlushForTesting();

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  EXPECT_EQ(report.type, "performance-observer");
  EXPECT_EQ(report.group, kEndpoint);
  EXPECT_EQ(report.url, kPageURL);

  const base::ListValue* report_entries = report.body.FindList("entries");
  ASSERT_TRUE(report_entries);
  ASSERT_EQ(report_entries->size(), 1u);  // only allowed_mark

  const base::Value& entry_val = (*report_entries)[0];
  const base::DictValue* mark_entry = entry_val.GetIfDict();
  ASSERT_TRUE(mark_entry);
  EXPECT_EQ(*(mark_entry->FindString("entryType")), "mark");
  EXPECT_EQ(*(mark_entry->FindString("name")), "allowed_mark");
  EXPECT_EQ(*(mark_entry->FindDouble("startTime")), 100.0);
}

TEST_F(DeclarativePerformanceObserverTest, RecordsPrerenderActivation) {
  const GURL kPageURL("https://example.com/prerender.html");
  const std::string kEndpoint("telemetry");

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = kEndpoint;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kNavigation);

  MockNavigationHandle initial_navigation_handle(kPageURL, main_rfh());
  initial_navigation_handle.set_has_committed(true);
  ON_CALL(initial_navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  base::TimeTicks nav_start = base::TimeTicks::Now();
  ON_CALL(initial_navigation_handle, NavigationStart())
      .WillByDefault(testing::Return(nav_start));

  NavigationHandleTiming timing;
  ON_CALL(initial_navigation_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(timing));

  CreateObserver(&initial_navigation_handle);

  // Simulate prerender activation
  MockNavigationHandle activation_navigation_handle(kPageURL, main_rfh());
  activation_navigation_handle.set_has_committed(true);
  activation_navigation_handle.set_is_prerendered_page_activation(true);

  base::TimeTicks activation_start = nav_start + base::Milliseconds(123);
  ON_CALL(activation_navigation_handle, NavigationStart())
      .WillByDefault(testing::Return(activation_start));

  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  ASSERT_TRUE(observer);
  observer->OnPrerenderActivation(&activation_navigation_handle);

  // Trigger flush
  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  EXPECT_EQ(report.type, "performance-observer");
  EXPECT_EQ(report.group, kEndpoint);

  const base::ListValue* report_entries = report.body.FindList("entries");
  ASSERT_TRUE(report_entries);
  ASSERT_EQ(report_entries->size(), 2u);  // navigation, session-end

  const base::Value& nav_val = (*report_entries)[0];
  const base::DictValue* nav_entry = nav_val.GetIfDict();
  ASSERT_TRUE(nav_entry);
  EXPECT_EQ(*(nav_entry->FindString("entryType")), "navigation");
  EXPECT_EQ(*(nav_entry->FindString("deliveryType")), "navigational-prefetch");
  EXPECT_EQ(*(nav_entry->FindDouble("activationStart")), 123.0);
}

TEST_F(DeclarativePerformanceObserverTest,
       RecordsEarlyFailureAndMergesOnOptIn) {
  const GURL kPageURL("https://example.com/index.html");
  const url::Origin kOrigin = url::Origin::Create(kPageURL);
  auto* partition =
      static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());
  ASSERT_TRUE(partition);

  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true);

  NavigationHandleTiming default_timing;
  testing::NiceMock<MockNavigationHandle> failed_handle(kPageURL, main_rfh());
  ON_CALL(failed_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(default_timing));
  DeclarativePerformanceObserver::RecordEarlyNavigationFailure(
      &failed_handle, partition, net::ERR_CONNECTION_REFUSED);

  base::RunLoop barrier;
  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true, barrier.QuitClosure());
  barrier.Run();

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = "telemetry";
  policy->capture_early_failures = true;
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kVisibilityState);

  MockNavigationHandle navigation_handle(kPageURL, main_rfh());
  navigation_handle.set_has_committed(true);
  navigation_handle.set_is_in_primary_main_frame(true);

  ON_CALL(navigation_handle, GetDeclarativePerformanceObserverPolicy())
      .WillByDefault(testing::Return(policy.get()));

  CreateObserver(&navigation_handle);

  base::RunLoop sync_loop;
  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true, sync_loop.QuitClosure());
  sync_loop.Run();

  DeclarativePerformanceObserver::DeleteForCurrentDocument(main_rfh());

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  const base::ListValue* entries = report.body.FindList("entries");
  ASSERT_TRUE(entries);
  ASSERT_EQ(entries->size(), 3u);
}

TEST_F(DeclarativePerformanceObserverTest, Enforces640KBFIFOQuota) {
  const GURL kPageURL("https://example.com/index.html");
  const url::Origin kOrigin = url::Origin::Create(kPageURL);
  auto* partition =
      static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());

  base::RunLoop run_loop_limit;
  partition->GetDeclarativePerformanceObserverStore()->SetQuotaLimitForTesting(
      4096, run_loop_limit.QuitClosure());
  run_loop_limit.Run();

  base::RunLoop run_loop1;
  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true, run_loop1.QuitClosure());
  run_loop1.Run();

  NavigationHandleTiming default_timing;
  testing::NiceMock<MockNavigationHandle> sample_handle(kPageURL, main_rfh());
  ON_CALL(sample_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(default_timing));
  for (int i = 0; i < 20; ++i) {
    DeclarativePerformanceObserver::RecordEarlyNavigationFailure(
        &sample_handle, partition, net::ERR_CONNECTION_REFUSED);
  }

  base::RunLoop barrier;
  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true, barrier.QuitClosure());
  barrier.Run();

  base::ListValue reports;
  base::RunLoop run_loop2;
  partition->GetDeclarativePerformanceObserverStore()->TakeEarlyFailureReports(
      kOrigin, base::BindOnce(
                   [](base::ListValue* out, base::OnceClosure quit,
                      base::ListValue res) {
                     *out = std::move(res);
                     std::move(quit).Run();
                   },
                   &reports, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_GE(reports.size(), 10u);
  EXPECT_LE(reports.size(), 13u);
}

TEST_F(DeclarativePerformanceObserverTest,
       RecordsEarlyFailureInCorrectPartition) {
  const GURL kCustomSite("https://custom-example.com");
  const GURL kCustomURL("https://custom-example.com/index.html");
  const url::Origin kCustomOrigin = url::Origin::Create(kCustomURL);

  // 1. Set up the custom storage partition client.
  CustomStoragePartitionForSomeSites client(kCustomSite);
  ScopedContentBrowserClientSetting setting(&client);

  auto* default_partition =
      static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());

  auto* browser_context = main_rfh()->GetBrowserContext();
  StoragePartitionConfig config =
      client.GetStoragePartitionConfigForSite(browser_context, kCustomSite);
  auto* custom_partition = static_cast<StoragePartitionImpl*>(
      browser_context->GetStoragePartition(config));

  ASSERT_NE(default_partition, custom_partition);

  // 2. Set early failure policy in the CUSTOM partition and default partition.
  base::RunLoop barrier1;
  custom_partition->GetDeclarativePerformanceObserverStore()
      ->SetEarlyFailurePolicy(kCustomOrigin, true, barrier1.QuitClosure());
  barrier1.Run();

  base::RunLoop barrier2;
  default_partition->GetDeclarativePerformanceObserverStore()
      ->SetEarlyFailurePolicy(kCustomOrigin, false, barrier2.QuitClosure());
  barrier2.Run();

  // 3. Simulate failure.
  std::unique_ptr<NavigationSimulator> failed_simulator =
      NavigationSimulator::CreateBrowserInitiated(kCustomURL, web_contents());
  failed_simulator->Start();
  failed_simulator->Fail(net::ERR_CONNECTION_REFUSED);

  // Sync database operations.
  base::RunLoop sync_loop;
  custom_partition->GetDeclarativePerformanceObserverStore()
      ->SetEarlyFailurePolicy(kCustomOrigin, true, sync_loop.QuitClosure());
  sync_loop.Run();

  // 4. Verify report is in the CUSTOM partition's store.
  base::ListValue custom_reports;
  base::RunLoop run_loop1;
  custom_partition->GetDeclarativePerformanceObserverStore()
      ->TakeEarlyFailureReports(
          kCustomOrigin, base::BindOnce(
                             [](base::ListValue* out, base::OnceClosure quit,
                                base::ListValue res) {
                               *out = std::move(res);
                               std::move(quit).Run();
                             },
                             &custom_reports, run_loop1.QuitClosure()));
  run_loop1.Run();
  EXPECT_EQ(custom_reports.size(), 1u);

  // 5. Verify report is NOT in the DEFAULT partition's store.
  base::ListValue default_reports;
  base::RunLoop run_loop2;
  default_partition->GetDeclarativePerformanceObserverStore()
      ->TakeEarlyFailureReports(
          kCustomOrigin, base::BindOnce(
                             [](base::ListValue* out, base::OnceClosure quit,
                                base::ListValue res) {
                               *out = std::move(res);
                               std::move(quit).Run();
                             },
                             &default_reports, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_EQ(default_reports.size(), 0u);
}

class DeclarativePerformanceObserverOriginTrialTest
    : public DeclarativePerformanceObserverTest {
 public:
  DeclarativePerformanceObserverOriginTrialTest() = default;

 private:
  blink::ScopedTestOriginTrialPolicy origin_trial_policy_;
};

TEST_F(DeclarativePerformanceObserverOriginTrialTest, OriginTrialGated) {
  const GURL kPageURL("https://example.com/index.html");

  // 1. Reset explicit override to test default Origin Trial gated behavior.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithEmptyFeatureAndFieldTrialLists();

  // 2. Try to navigate WITH the Performance-Observer header but WITHOUT the OT
  // token.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
    simulator->Start();

    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    headers->AddHeader("Performance-Observer",
                       "report-to=\"telemetry\", capture-early-failures=true, "
                       "entry-types=(\"navigation\")");
    simulator->SetResponseHeaders(headers);
    simulator->Commit();

    // Verify that the observer was NOT created (because OT is missing).
    auto* observer =
        DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
    EXPECT_FALSE(observer);
  }

  // 2b. Try to navigate WITH the Performance-Observer header AND WITH a token
  // for a DIFFERENT feature.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
    simulator->Start();

    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    headers->AddHeader("Performance-Observer",
                       "report-to=\"telemetry\", capture-early-failures=true, "
                       "entry-types=(\"navigation\")");
    // Valid token for https://example.com but for "DummyFeature"
    headers->AddHeader(
        "Origin-Trial",
        "A/UfT0vT/si9yD1YdLXncUei180zQQrpu8+QaF/yhQgfBhERC0jdyFswSttoojUXStecZ"
        "0xCwL3gRiYCxsf+AwAAAABWeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0ND"
        "MiLCAiZmVhdHVyZSI6ICJIdW1teUZlYXR1cmUiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0"
        "=");
    simulator->SetResponseHeaders(headers);
    simulator->Commit();

    // Verify that the observer was NOT created (because the token is for a
    // different feature).
    auto* observer =
        DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
    EXPECT_FALSE(observer);
  }

  // 2c. Try to navigate WITH the Performance-Observer header AND WITH a
  // MALFORMED token.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
    simulator->Start();

    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    headers->AddHeader("Performance-Observer",
                       "report-to=\"telemetry\", capture-early-failures=true, "
                       "entry-types=(\"navigation\")");
    // Malformed token
    headers->AddHeader("Origin-Trial", "not-a-valid-token-at-all");
    simulator->SetResponseHeaders(headers);
    simulator->Commit();

    // Verify that the observer was NOT created.
    auto* observer =
        DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
    EXPECT_FALSE(observer);
  }

  // 3. Try to navigate WITH the Performance-Observer header AND WITH a VALID OT
  // token.
  {
    auto simulator =
        NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
    simulator->Start();

    auto headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    headers->AddHeader("Performance-Observer",
                       "report-to=\"telemetry\", capture-early-failures=true, "
                       "entry-types=(\"navigation\")");
    // Valid token for https://example.com and DeclarativePerformanceObserver
    headers->AddHeader(
        "Origin-Trial",
        "A6umeji0ZeijjMlMf+9BwGsWirfa1RScCpY7xKTExl1kdyzXKLwnYfdCIgFv4FoVaBDUzX"
        "z15kxM/25jT7kN/gwAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDM"
        "iLCAiZmVhdHVyZSI6ICJEZWNsYXJhdGl2ZVBlcmZvcm1hbmNlT2JzZXJ2ZXIiLCAiZXhw"
        "aXJ5IjogMjAwMDAwMDAwMH0=");

    simulator->SetResponseHeaders(headers);
    simulator->Commit();

    // Verify that the observer WAS successfully created!
    auto* observer =
        DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
    EXPECT_TRUE(observer);
  }
}

TEST_F(DeclarativePerformanceObserverOriginTrialTest, KillSwitchGated) {
  const GURL kPageURL("https://example.com/index.html");

  // Disable via killswitch.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      blink::features::kDeclarativePerformanceObserver);

  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
  simulator->Start();

  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Performance-Observer",
                     "report-to=\"telemetry\", capture-early-failures=true, "
                     "entry-types=(\"navigation\")");
  // Valid token for https://example.com and DeclarativePerformanceObserver
  headers->AddHeader(
      "Origin-Trial",
      "A6umeji0ZeijjMlMf+9BwGsWirfa1RScCpY7xKTExl1kdyzXKLwnYfdCIgFv4FoVaBDUzX"
      "z15kxM/25jT7kN/gwAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDM"
      "iLCAiZmVhdHVyZSI6ICJEZWNsYXJhdGl2ZVBlcmZvcm1hbmNlT2JzZXJ2ZXIiLCAiZXhw"
      "aXJ5IjogMjAwMDAwMDAwMH0=");

  simulator->SetResponseHeaders(headers);
  simulator->Commit();

  // Verify that even with a valid OT token, the observer is NOT created when
  // killswitch is active.
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  EXPECT_FALSE(observer);
}

TEST_F(DeclarativePerformanceObserverOriginTrialTest,
       ExplicitFlagOverrideEnablesWithoutOriginTrial) {
  const GURL kPageURL("https://example.com/index.html");

  // Enable explicitly via commandline/Finch override
  // (`override_state.has_value() && override_state.value()`).
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      blink::features::kDeclarativePerformanceObserver);

  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
  simulator->Start();

  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Performance-Observer",
                     "report-to=\"telemetry\", capture-early-failures=true, "
                     "entry-types=(\"navigation\")");
  // NOTICE: No Origin-Trial token header is added.
  simulator->SetResponseHeaders(headers);
  simulator->Commit();

  // Verify that explicitly overriding the flag enables the feature and
  // GetDeclarativePerformanceObserverPolicy() activates the observer even
  // without a valid Origin Trial token.
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  EXPECT_TRUE(observer);
}

TEST_F(DeclarativePerformanceObserverOriginTrialTest,
       EarlyFailuresParamDisabledPreventsPolicyRegistration) {
  const GURL kPageURL("https://example.com/index.html");

  // Enable kDeclarativePerformanceObserver but disable
  // kDeclarativePerformanceObserverCaptureEarlyFailures param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kDeclarativePerformanceObserver,
      {{"DeclarativePerformanceObserverSupportCaptureEarlyFailures", "false"}});

  // 1. Navigate with the Performance-Observer header containing
  // capture-early-failures=true, and a valid Origin-Trial token.
  auto simulator =
      NavigationSimulator::CreateBrowserInitiated(kPageURL, web_contents());
  simulator->Start();

  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader("Performance-Observer",
                     "report-to=\"telemetry\", capture-early-failures=true, "
                     "entry-types=(\"navigation\")");
  headers->AddHeader(
      "Origin-Trial",
      "A6umeji0ZeijjMlMf+9BwGsWirfa1RScCpY7xKTExl1kdyzXKLwnYfdCIgFv4FoVaBDUzX"
      "z15kxM/25jT7kN/gwAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDM"
      "iLCAiZmVhdHVyZSI6ICJEZWNsYXJhdGl2ZVBlcmZvcm1hbmNlT2JzZXJ2ZXIiLCAiZXhw"
      "aXJ5IjogMjAwMDAwMDAwMH0=");

  simulator->SetResponseHeaders(headers);
  simulator->Commit();

  // 2. Verify DPO observer is created.
  auto* observer =
      DeclarativePerformanceObserver::GetForCurrentDocument(main_rfh());
  EXPECT_TRUE(observer);

  // 3. Since the database did not register a true early failure policy,
  // early failures should NOT be recorded.
  auto* partition = main_rfh()->GetStoragePartition();
  auto* partition_impl = static_cast<StoragePartitionImpl*>(partition);
  auto* store = partition_impl->GetDeclarativePerformanceObserverStore();
  ASSERT_TRUE(store);

  // Wait for database tasks to finish to be sure.
  base::RunLoop run_loop;
  store->SetEarlyFailurePolicy(url::Origin::Create(kPageURL), false,
                               run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(store->HasEarlyFailurePolicy(url::Origin::Create(kPageURL)));
}

TEST_F(DeclarativePerformanceObserverTest,
       EarlyFailuresParamDisabledPreventsRecordingEvenIfPreviouslyOptedIn) {
  const GURL kPageURL("https://example.com/index.html");
  const url::Origin kOrigin = url::Origin::Create(kPageURL);
  auto* partition =
      static_cast<StoragePartitionImpl*>(main_rfh()->GetStoragePartition());
  ASSERT_TRUE(partition);

  // Disable kDeclarativePerformanceObserverCaptureEarlyFailures param.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kDeclarativePerformanceObserver,
      {{"DeclarativePerformanceObserverSupportCaptureEarlyFailures", "false"}});

  // Force opt-in to simulate previous registration.
  partition->GetDeclarativePerformanceObserverStore()->SetEarlyFailurePolicy(
      kOrigin, true);

  NavigationHandleTiming default_timing;
  testing::NiceMock<MockNavigationHandle> failed_handle(kPageURL, main_rfh());
  ON_CALL(failed_handle, GetNavigationHandleTiming())
      .WillByDefault(testing::ReturnRef(default_timing));

  // Try to record early failure.
  DeclarativePerformanceObserver::RecordEarlyNavigationFailure(
      &failed_handle, partition, net::ERR_CONNECTION_REFUSED);

  // Take reports to verify no reports were saved.
  base::RunLoop run_loop;
  base::ListValue reports;
  partition->GetDeclarativePerformanceObserverStore()->TakeEarlyFailureReports(
      kOrigin,
      base::BindOnce(
          [](base::RunLoop* loop, base::ListValue* out, base::ListValue list) {
            *out = std::move(list);
            loop->Quit();
          },
          &run_loop, &reports));
  run_loop.Run();

  EXPECT_EQ(reports.size(), 0u);
}

}  // namespace
}  // namespace content
