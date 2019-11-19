// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <unistd.h>

#include <utility>

#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

const uint64_t kTestBlinkThreshold = 80 * 1024;
const uint64_t kTestPMFThreshold = 160 * 1024;
const uint64_t kTestSwapThreshold = 500 * 1024;
const uint64_t kTestVmSizeThreshold = 1024 * 1024;

class MockOomInterventionHost : public mojom::blink::OomInterventionHost {
 public:
  MockOomInterventionHost(
      mojo::PendingReceiver<mojom::blink::OomInterventionHost> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockOomInterventionHost() override = default;

  void OnHighMemoryUsage() override {}

 private:
  mojo::Receiver<mojom::blink::OomInterventionHost> receiver_;
};

// Mock that allows setting mock memory usage.
class MockMemoryUsageMonitor : public MemoryUsageMonitor {
 public:
  MockMemoryUsageMonitor() = default;

  MemoryUsage GetCurrentMemoryUsage() override { return mock_memory_usage_; }

  // MemoryUsageMonitor will report the current memory usage as this value.
  void SetMockMemoryUsage(MemoryUsage usage) { mock_memory_usage_ = usage; }

 private:
  MemoryUsage mock_memory_usage_;
};

// Mock intervention class that uses a mock MemoryUsageMonitor.
class MockOomInterventionImpl : public OomInterventionImpl {
 public:
  MockOomInterventionImpl()
      : mock_memory_usage_monitor_(std::make_unique<MockMemoryUsageMonitor>()) {
  }
  ~MockOomInterventionImpl() override {}

  MemoryUsageMonitor& MemoryUsageMonitorInstance() override {
    return *mock_memory_usage_monitor_;
  }

  MockMemoryUsageMonitor* mock_memory_usage_monitor() {
    return mock_memory_usage_monitor_.get();
  }

 private:
  std::unique_ptr<OomInterventionMetrics> metrics_;
  std::unique_ptr<MockMemoryUsageMonitor> mock_memory_usage_monitor_;
};

}  // namespace

class OomInterventionImplTest : public testing::Test {
 public:
  void SetUp() override {
    intervention_ = std::make_unique<MockOomInterventionImpl>();
  }

  Page* DetectOnceOnBlankPage() {
    WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about:blank");
    Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();
    EXPECT_FALSE(page->Paused());
    RunDetection(true, false, false);
    return page;
  }

  void RunDetection(bool renderer_pause_enabled,
                    bool navigate_ads_enabled,
                    bool purge_v8_memory_enabled) {
    mojo::PendingRemote<mojom::blink::OomInterventionHost> remote_host;
    MockOomInterventionHost mock_host(
        remote_host.InitWithNewPipeAndPassReceiver());

    mojom::blink::DetectionArgsPtr args(mojom::blink::DetectionArgs::New());
    args->blink_workload_threshold = kTestBlinkThreshold;
    args->private_footprint_threshold = kTestPMFThreshold;
    args->swap_threshold = kTestSwapThreshold;
    args->virtual_memory_thresold = kTestVmSizeThreshold;

    intervention_->StartDetection(std::move(remote_host), std::move(args),
                                  renderer_pause_enabled, navigate_ads_enabled,
                                  purge_v8_memory_enabled);
    test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  }

 protected:
  std::unique_ptr<MockOomInterventionImpl> intervention_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  std::unique_ptr<SimRequest> main_resource_;
};

TEST_F(OomInterventionImplTest, NoDetectionOnBelowThreshold) {
  MemoryUsage usage;
  // Set value less than the threshold to not trigger intervention.
  usage.v8_bytes = kTestBlinkThreshold - 1024;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = kTestPMFThreshold - 1024;
  usage.swap_bytes = kTestSwapThreshold - 1024;
  usage.vm_size_bytes = kTestVmSizeThreshold - 1024;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, BlinkThresholdDetection) {
  MemoryUsage usage;
  // Set value more than the threshold to trigger intervention.
  usage.v8_bytes = kTestBlinkThreshold + 1024;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, PmfThresholdDetection) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.private_footprint_bytes = kTestPMFThreshold + 1024;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, SwapThresholdDetection) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.swap_bytes = kTestSwapThreshold + 1024;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, VmSizeThresholdDetection) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.vm_size_bytes = kTestVmSizeThreshold + 1024;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, StopWatchingAfterDetection) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.blink_gc_bytes = kTestBlinkThreshold + 1024;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  DetectOnceOnBlankPage();

  EXPECT_FALSE(intervention_->mock_memory_usage_monitor()->HasObserver(
      intervention_.get()));
}

TEST_F(OomInterventionImplTest, ContinueWatchingWithoutDetection) {
  MemoryUsage usage;
  // Set value less than the threshold to not trigger intervention.
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  DetectOnceOnBlankPage();

  EXPECT_TRUE(intervention_->mock_memory_usage_monitor()->HasObserver(
      intervention_.get()));
}

// TODO(yuzus): Once OOPIF unit test infrastructure is ready, add a test case
// with OOPIF enabled.
TEST_F(OomInterventionImplTest, V1DetectionAdsNavigation) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.partition_alloc_bytes = kTestBlinkThreshold + 1024;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about:blank");
  Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();

  web_view->MainFrameImpl()
      ->GetFrame()
      ->GetDocument()
      ->body()
      ->SetInnerHTMLFromString(
          "<iframe name='ad' src='data:text/html,'></iframe><iframe "
          "name='non-ad' src='data:text/html,'>");

  WebFrame* ad_iframe = web_view_helper_.LocalMainFrame()->FindFrameByName(
      WebString::FromUTF8("ad"));
  WebFrame* non_ad_iframe = web_view_helper_.LocalMainFrame()->FindFrameByName(
      WebString::FromUTF8("non-ad"));

  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      ad_iframe->ToWebLocalFrame());
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      non_ad_iframe->ToWebLocalFrame());

  auto* local_adframe = To<LocalFrame>(WebFrame::ToCoreFrame(*ad_iframe));
  local_adframe->SetIsAdSubframe(blink::mojom::AdFrameType::kRootAd);
  auto* local_non_adframe =
      To<LocalFrame>(WebFrame::ToCoreFrame(*non_ad_iframe));

  EXPECT_TRUE(local_adframe->IsAdSubframe());
  EXPECT_FALSE(local_non_adframe->IsAdSubframe());
  EXPECT_EQ(local_adframe->GetDocument()->Url().GetString(), "data:text/html,");
  EXPECT_EQ(local_non_adframe->GetDocument()->Url().GetString(),
            "data:text/html,");

  RunDetection(true, true, false);

  EXPECT_TRUE(page->Paused());
  intervention_.reset();

  // The about:blank navigation won't actually happen until the page unpauses.
  frame_test_helpers::PumpPendingRequestsForFrameToLoad(
      ad_iframe->ToWebLocalFrame());
  EXPECT_EQ(local_adframe->GetDocument()->Url().GetString(), "about:blank");
  EXPECT_NE(local_non_adframe->GetDocument()->Url().GetString(), "about:blank");
}

TEST_F(OomInterventionImplTest, V2DetectionV8PurgeMemory) {
  MemoryUsage usage;
  usage.v8_bytes = 0;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = 0;
  usage.swap_bytes = 0;
  // Set value more than the threshold to trigger intervention.
  usage.vm_size_bytes = kTestVmSizeThreshold + 1024;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about:blank");
  Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();
  auto* frame = To<LocalFrame>(page->MainFrame());
  EXPECT_FALSE(frame->GetDocument()->ExecutionContext::IsContextDestroyed());
  RunDetection(true, true, true);
  EXPECT_TRUE(frame->GetDocument()->ExecutionContext::IsContextDestroyed());
}

TEST_F(OomInterventionImplTest, ReducedMemoryMetricReporting) {
  HistogramTester histogram_tester;

  uint64_t initial_blink_usage_bytes = kTestBlinkThreshold + 1024 * 1024 * 1024;
  uint64_t initial_private_footprint_bytes = 0;

  MemoryUsage usage;
  // Set value more than the threshold to trigger intervention.
  usage.v8_bytes = initial_blink_usage_bytes;
  usage.blink_gc_bytes = 0;
  usage.partition_alloc_bytes = 0;
  usage.private_footprint_bytes = initial_private_footprint_bytes;
  usage.swap_bytes = 0;
  usage.vm_size_bytes = 0;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());

  usage.v8_bytes = initial_blink_usage_bytes - 2 * 1024 * 1024;
  usage.private_footprint_bytes =
      initial_private_footprint_bytes + 2 * 1024 * 1024;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);
  test::RunDelayedTasks(base::TimeDelta::FromSeconds(10));
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter10secs2", 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter10secs2", -2,
      1);

  usage.v8_bytes = initial_blink_usage_bytes - 1;
  usage.private_footprint_bytes = initial_private_footprint_bytes + 1;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);
  test::RunDelayedTasks(base::TimeDelta::FromSeconds(10));
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter20secs2", 0,
      1);
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter20secs2", 0,
      1);

  usage.v8_bytes = initial_blink_usage_bytes - 800 * 1024 * 1024;
  usage.private_footprint_bytes =
      initial_private_footprint_bytes + 800 * 1024 * 1024;
  intervention_->mock_memory_usage_monitor()->SetMockMemoryUsage(usage);
  test::RunDelayedTasks(base::TimeDelta::FromSeconds(10));
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedBlinkUsageAfter30secs2", 500,
      1);
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.OomIntervention.ReducedRendererPMFAfter30secs2",
      -500, 1);
}

}  // namespace blink
