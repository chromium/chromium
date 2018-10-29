// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <unistd.h>

#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/binding.h"
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
  MockOomInterventionHost(mojom::blink::OomInterventionHostRequest request)
      : binding_(this, std::move(request)) {}
  ~MockOomInterventionHost() override = default;

  void OnHighMemoryUsage() override {}

 private:
  mojo::Binding<mojom::blink::OomInterventionHost> binding_;
};

// Mock intervention class that has custom method for fetching metrics.
class MockOomInterventionImpl : public OomInterventionImpl {
 public:
  MockOomInterventionImpl() {}
  ~MockOomInterventionImpl() override {}

  // If metrics are set by calling this method, then GetCurrentMemoryMetrics()
  // will return the given metrics, else it will calculate metrics from
  // providers.
  void SetMetrics(OomInterventionMetrics metrics) {
    metrics_ = std::make_unique<OomInterventionMetrics>();
    *metrics_ = metrics;
  }

 private:
  OomInterventionMetrics GetCurrentMemoryMetrics() override {
    if (metrics_)
      return *metrics_;
    return CrashMemoryMetricsReporterImpl::Instance().GetCurrentMemoryMetrics();
  }

  std::unique_ptr<OomInterventionMetrics> metrics_;
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
    RunDetection(true, false);
    return page;
  }

  void RunDetection(bool renderer_pause_enabled, bool navigate_ads_enabled) {
    mojom::blink::OomInterventionHostPtr host_ptr;
    MockOomInterventionHost mock_host(mojo::MakeRequest(&host_ptr));

    mojom::blink::DetectionArgsPtr args(mojom::blink::DetectionArgs::New());
    args->blink_workload_threshold = kTestBlinkThreshold;
    args->private_footprint_threshold = kTestPMFThreshold;
    args->swap_threshold = kTestSwapThreshold;
    args->virtual_memory_thresold = kTestVmSizeThreshold;

    intervention_->StartDetection(std::move(host_ptr), std::move(args),
                                  renderer_pause_enabled, navigate_ads_enabled);
    test::RunDelayedTasks(TimeDelta::FromSeconds(1));
  }

 protected:
  std::unique_ptr<MockOomInterventionImpl> intervention_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  std::unique_ptr<SimRequest> main_resource_;
};

TEST_F(OomInterventionImplTest, NoDetectionOnBelowThreshold) {
  OomInterventionMetrics mock_metrics = {};
  // Set value less than the threshold to not trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, BlinkThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  // Set value more than the threshold to not trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) + 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, PmfThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  // Set value more than the threshold to trigger intervention.
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) + 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, SwapThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  // Set value more than the threshold to trigger intervention.
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) + 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, VmSizeThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  // Set value more than the threshold to trigger intervention.
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) + 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, StopWatchingAfterDetection) {
  OomInterventionMetrics mock_metrics = {};
  // Set value more than the threshold to trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) + 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  DetectOnceOnBlankPage();

  EXPECT_FALSE(intervention_->timer_.IsActive());
}

TEST_F(OomInterventionImplTest, ContinueWatchingWithoutDetection) {
  OomInterventionMetrics mock_metrics = {};
  // Set value less than the threshold to not trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  DetectOnceOnBlankPage();

  EXPECT_TRUE(intervention_->timer_.IsActive());
}

TEST_F(OomInterventionImplTest, CalculateProcessFootprint) {
  const char kStatusFile[] =
      "First:  1\n Second: 2 kB\nVmSwap: 10 kB \n Third: 10 kB\n Last: 8";
  const char kStatmFile[] = "100 40 25 0 0";
  uint64_t expected_swap_kb = 10;
  uint64_t expected_private_footprint_kb =
      (40 - 25) * getpagesize() / 1024 + expected_swap_kb;
  uint64_t expected_vm_size_kb = 100 * getpagesize() / 1024;

  base::FilePath statm_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&statm_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatmFile)),
            base::WriteFile(statm_path, kStatmFile, sizeof(kStatmFile)));
  base::File statm_file(statm_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::FilePath status_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&status_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatusFile)),
            base::WriteFile(status_path, kStatusFile, sizeof(kStatusFile)));
  base::File status_file(status_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  CrashMemoryMetricsReporterImpl::Instance().statm_fd_.reset(
      statm_file.TakePlatformFile());
  CrashMemoryMetricsReporterImpl::Instance().status_fd_.reset(
      status_file.TakePlatformFile());

  mojom::blink::OomInterventionHostPtr host_ptr;
  MockOomInterventionHost mock_host(mojo::MakeRequest(&host_ptr));
  mojom::blink::DetectionArgsPtr args(mojom::blink::DetectionArgs::New());
  intervention_->StartDetection(std::move(host_ptr), std::move(args),
                                true /*renderer_pause_enabled*/,
                                true /*navigate_ads_enabled*/);
  // Create unsafe shared memory region to write metrics in reporter.
  base::UnsafeSharedMemoryRegion shm =
      base::UnsafeSharedMemoryRegion::Create(sizeof(OomInterventionMetrics));
  CrashMemoryMetricsReporterImpl::Instance().shared_metrics_mapping_ =
      shm.Map();
  EXPECT_TRUE(CrashMemoryMetricsReporterImpl::Instance()
                  .shared_metrics_mapping_.IsValid());

  intervention_->Check(nullptr);
  OomInterventionMetrics* metrics = static_cast<OomInterventionMetrics*>(
      CrashMemoryMetricsReporterImpl::Instance()
          .shared_metrics_mapping_.memory());
  EXPECT_EQ(expected_private_footprint_kb,
            metrics->current_private_footprint_kb);
  EXPECT_EQ(expected_swap_kb, metrics->current_swap_kb);
  EXPECT_EQ(expected_vm_size_kb, metrics->current_vm_size_kb);
}

// TODO(yuzus): Once OOPIF unit test infrastructure is ready, add a test case
// with OOPIF enabled.
TEST_F(OomInterventionImplTest, V1DetectionAdsNavigation) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  // Set value more than the threshold to trigger intervention.
  mock_metrics.current_vm_size_kb = (kTestVmSizeThreshold / 1024) + 1;
  intervention_->SetMetrics(mock_metrics);

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

  LocalFrame* local_adframe = ToLocalFrame(WebFrame::ToCoreFrame(*ad_iframe));
  local_adframe->SetIsAdSubframe();
  LocalFrame* local_non_adframe =
      ToLocalFrame(WebFrame::ToCoreFrame(*non_ad_iframe));

  EXPECT_TRUE(local_adframe->IsAdSubframe());
  EXPECT_FALSE(local_non_adframe->IsAdSubframe());

  RunDetection(true, true);

  EXPECT_EQ(local_adframe->GetDocument()->Url().GetString(), "about:blank");
  EXPECT_NE(local_non_adframe->GetDocument()->Url().GetString(), "about:blank");
  EXPECT_TRUE(page->Paused());
}

}  // namespace blink
