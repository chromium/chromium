// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/declarative_performance_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  DeclarativePerformanceObserverTest() = default;
  ~DeclarativePerformanceObserverTest() override = default;

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    DeclarativePerformanceObserver::CreateForWebContents(web_contents());
    storage_partition_.set_network_context(&network_context_);

    DeclarativePerformanceObserver::FromWebContents(web_contents())
        ->SetStoragePartitionForTesting(&storage_partition_);
  }

  content::TestStoragePartition storage_partition_;
  TestNetworkContext network_context_;
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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->OnVisibilityChanged(Visibility::HIDDEN);

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->OnVisibilityChanged(Visibility::VISIBLE);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

  // Trigger unload to flush metrics
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

  // 2. Enter BackForwardCache
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameHostStateChanged(
          main_rfh(), RenderFrameHost::LifecycleState::kActive,
          RenderFrameHost::LifecycleState::kInBackForwardCache);

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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&restore_handle);

  // Trigger unload to flush metrics for the RESTORED session
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

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
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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

  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->DidFinishNavigation(&navigation_handle);

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
  DeclarativePerformanceObserver::FromWebContents(web_contents())
      ->RenderFrameDeleted(main_rfh());

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

}  // namespace
}  // namespace content
