// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/crash_report_utils.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/kill.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/network_anonymization_key.h"
#include "partition_alloc/page_allocator.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#endif

namespace content::crash_report {
namespace {

base::UnsafeSharedMemoryRegion CreateRegionWithPayload(
    std::string_view payload,
    std::optional<uint32_t> override_size = std::nullopt) {
  const uint32_t recorded_size =
      override_size.value_or(static_cast<uint32_t>(payload.size()));
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(sizeof(uint32_t) + payload.size());
  base::WritableSharedMemoryMapping mapping = region.Map();
  base::SpanWriter<uint8_t> writer(mapping.GetMemoryAsSpan<uint8_t>());
  writer.WriteU32NativeEndian(recorded_size);
  writer.Write(base::as_byte_span(payload));
  return region;
}

class CrashMetricsMockRenderProcessHost : public MockRenderProcessHost {
 public:
  explicit CrashMetricsMockRenderProcessHost(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {}

  void SetCrashMemoryMetrics(blink::OomInterventionMetrics metrics) {
    crash_memory_metrics_ = metrics;
  }

  std::optional<blink::OomInterventionMetrics> GetCrashMemoryMetrics()
      const override {
    return crash_memory_metrics_;
  }

 private:
  std::optional<blink::OomInterventionMetrics> crash_memory_metrics_;
};

class RecordingNetworkContext : public network::TestNetworkContext {
 public:
  struct QueuedReport {
    std::string type;
    std::string group;
    GURL url;
    std::optional<base::UnguessableToken> reporting_source;
    base::DictValue body;
  };

  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::DictValue body) override {
    reports_.push_back(
        QueuedReport{type, group, url, reporting_source, std::move(body)});
  }

  mojo::PendingRemote<network::mojom::NetworkContext>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  const std::vector<QueuedReport>& reports() const { return reports_; }

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_{this};
  std::vector<QueuedReport> reports_;
};

class CrashReportUtilsTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    auto* storage_partition = static_cast<StoragePartitionImpl*>(
        browser_context()->GetDefaultStoragePartition());
    storage_partition->SetNetworkContextForTesting(
        network_context_.BindNewPipeAndPassRemote());
  }

  RenderFrameHostImpl* main_rfh_impl() {
    return static_cast<RenderFrameHostImpl*>(main_rfh());
  }

  void FlushNetworkInterface() {
    browser_context()
        ->GetDefaultStoragePartition()
        ->FlushNetworkInterfaceForTesting();
  }

  RecordingNetworkContext network_context_;
};

TEST(CrashReportBodyTest, InvalidOrUndersizedRegionReturnsEmptyDict) {
  EXPECT_TRUE(ReadAPIBody(base::UnsafeSharedMemoryRegion()).empty());

  base::UnsafeSharedMemoryRegion two_byte_region =
      base::UnsafeSharedMemoryRegion::Create(2);
  EXPECT_TRUE(ReadAPIBody(two_byte_region).empty());
}

TEST(CrashReportBodyTest, OversizedHeaderLengthReturnsEmptyDict) {
  base::UnsafeSharedMemoryRegion region =
      CreateRegionWithPayload(R"({"k":"v"})", /*override_size=*/100);
  EXPECT_TRUE(ReadAPIBody(region).empty());
}

TEST(CrashReportBodyTest, MalformedOrNonDictJsonReturnsEmptyDict) {
  EXPECT_TRUE(ReadAPIBody(CreateRegionWithPayload("{not valid json")).empty());
  EXPECT_TRUE(ReadAPIBody(CreateRegionWithPayload(R"(["a", "b"])")).empty());
}

TEST(CrashReportBodyTest, ValidJsonExtractsOnlyStringEntries) {
  base::UnsafeSharedMemoryRegion region =
      CreateRegionWithPayload(R"({"str1":"val1","num":42,"flag":true,)"
                              R"("nested":{"a":"b"},"str2":"val2"})");

  base::DictValue dict = ReadAPIBody(region);
  EXPECT_EQ(dict.size(), 2u);
  EXPECT_EQ(*dict.FindString("str1"), "val1");
  EXPECT_EQ(*dict.FindString("str2"), "val2");
}

TEST_F(CrashReportUtilsTest, DetermineReasonClassification) {
  CrashMetricsMockRenderProcessHost process(browser_context());

  // Normal exit is not reported.
  ChildProcessTerminationInfo normal_info;
  normal_info.status = base::TERMINATION_STATUS_NORMAL_TERMINATION;
  normal_info.exit_code = RESULT_CODE_NORMAL_EXIT;
  EXPECT_EQ(DetermineReason(process, normal_info), std::nullopt);

  // Generic crash reports empty reason.
  ChildProcessTerminationInfo crash_info;
  crash_info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_info.exit_code = 1;
  EXPECT_EQ(DetermineReason(process, crash_info), "");

  // Explicit OOM termination statuses.
  ChildProcessTerminationInfo oom_info;
  oom_info.status = base::TERMINATION_STATUS_OOM;
  EXPECT_EQ(DetermineReason(process, oom_info), "oom");

  oom_info.status = base::TERMINATION_STATUS_EVICTED_FOR_MEMORY;
  EXPECT_EQ(DetermineReason(process, oom_info), "oom");

  // PartitionAlloc commit failure exit code attributes crash to OOM.
  ChildProcessTerminationInfo pa_oom_info;
  pa_oom_info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  pa_oom_info.exit_code =
      static_cast<int>(partition_alloc::kTerminateOnCommitFailureExitCode);
  EXPECT_EQ(DetermineReason(process, pa_oom_info), "oom");

  // Hung exit code attributes kill to unresponsive.
  ChildProcessTerminationInfo hung_info;
  hung_info.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  hung_info.exit_code = RESULT_CODE_HUNG;
  EXPECT_EQ(DetermineReason(process, hung_info), "unresponsive");

  // OOM metrics take precedence over unresponsive state.
  blink::OomInterventionMetrics oom_metrics;
  oom_metrics.allocation_failed = 1;
  process.SetCrashMemoryMetrics(oom_metrics);
  EXPECT_EQ(DetermineReason(process, hung_info), "oom");
}

TEST_F(CrashReportUtilsTest, DetermineReasonProcessWasKilled) {
  CrashMetricsMockRenderProcessHost process(browser_context());

  // Intentional kill (e.g. Task Manager) is not reported.
  ChildProcessTerminationInfo killed_info;
  killed_info.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  killed_info.exit_code = RESULT_CODE_KILLED;
  EXPECT_EQ(DetermineReason(process, killed_info), std::nullopt);

  // Bad-message kill is reported as a generic crash.
  killed_info.exit_code = RESULT_CODE_KILLED_BAD_MESSAGE;
  EXPECT_EQ(DetermineReason(process, killed_info), "");

#if BUILDFLAG(IS_POSIX)
  // SIGTERM is not reported, whereas SIGKILL is reported as a generic crash.
  killed_info.exit_code = SIGTERM;
  EXPECT_EQ(DetermineReason(process, killed_info), std::nullopt);

  killed_info.exit_code = SIGKILL;
  EXPECT_EQ(DetermineReason(process, killed_info), "");
#endif
}

TEST_F(CrashReportUtilsTest, QueuesReportWithStorageContextAndDeduplicates) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {blink::features::kCrashReportingStorageAPI,
       blink::features::kCrashReportingAPIMoreContextData},
      {});

  const GURL kUrl("https://example.com/app");
  NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(), kUrl);

  base::UnsafeSharedMemoryRegion writer_region;
  InitializeContext(*main_rfh_impl(), /*length=*/128,
                    base::BindOnce(
                        [](base::UnsafeSharedMemoryRegion* out,
                           base::UnsafeSharedMemoryRegion region) {
                          *out = std::move(region);
                        },
                        &writer_region));
  ASSERT_TRUE(writer_region.IsValid());

  constexpr std::string_view kJson = R"({"stage":"checkout"})";
  base::WritableSharedMemoryMapping mapping = writer_region.Map();
  base::SpanWriter<uint8_t> writer(mapping.GetMemoryAsSpan<uint8_t>());
  writer.WriteU32NativeEndian(static_cast<uint32_t>(kJson.size()));
  writer.Write(base::as_byte_span(kJson));

  ChildProcessTerminationInfo crash_info;
  crash_info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_info.exit_code = 1;

  MaybeGenerate(*main_rfh_impl(), crash_info);
  // A second invocation for the same live RenderFrame must be ignored.
  MaybeGenerate(*main_rfh_impl(), crash_info);
  FlushNetworkInterface();

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const auto& report = network_context_.reports()[0];
  EXPECT_EQ(report.type, "crash");
  EXPECT_EQ(report.group, "default");
  EXPECT_EQ(report.url, kUrl);
  EXPECT_EQ(report.body.FindBool("is_top_level"), true);
  EXPECT_NE(report.body.FindString("visibility_state"), nullptr);
  EXPECT_EQ(report.body.FindString("reason"), nullptr);

  const base::DictValue* api_body = report.body.FindDict("crash_report_api");
  ASSERT_NE(api_body, nullptr);
  EXPECT_EQ(*api_body->FindString("stage"), "checkout");
}

TEST_F(CrashReportUtilsTest, NonHttpOrHttpsDocumentDoesNotQueueReport) {
  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("data:text/html,hello"));

  ChildProcessTerminationInfo crash_info;
  crash_info.status = base::TERMINATION_STATUS_PROCESS_CRASHED;
  crash_info.exit_code = 1;

  MaybeGenerate(*main_rfh_impl(), crash_info);
  FlushNetworkInterface();

  EXPECT_TRUE(network_context_.reports().empty());
}

TEST_F(CrashReportUtilsTest, UnresponsiveCallStackAttributedByFrameToken) {
  base::test::ScopedFeatureList feature_list{
      blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports};

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.com/hung"));

  process()->SetUnresponsiveDocumentJSCallStackAndToken(
      "at busyLoop (app.js:10:5)", main_rfh()->GetFrameToken());

  ChildProcessTerminationInfo hung_info;
  hung_info.status = base::TERMINATION_STATUS_PROCESS_WAS_KILLED;
  hung_info.exit_code = RESULT_CODE_HUNG;

  MaybeGenerate(*main_rfh_impl(), hung_info);
  FlushNetworkInterface();

  ASSERT_EQ(network_context_.reports().size(), 1u);
  const base::DictValue& body = network_context_.reports()[0].body;
  EXPECT_EQ(*body.FindString("reason"), "unresponsive");
  EXPECT_EQ(*body.FindString("stack"), "at busyLoop (app.js:10:5)");
}

}  // namespace
}  // namespace content::crash_report
