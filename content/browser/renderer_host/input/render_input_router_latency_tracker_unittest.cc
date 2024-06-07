// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/render_input_router_latency_tracker.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/events/base_event_utils.h"

using base::Bucket;
using blink::WebInputEvent;
using testing::ElementsAre;

namespace content {
namespace {

// Trace ids are generated in sequence in practice, but in these tests, we don't
// care about the value, so we'll just use a constant.
const char kUrl[] = "http://www.foo.bar.com/subpage/1";

void AddFakeComponentsWithTimeStamp(
    const RenderInputRouterLatencyTracker& tracker,
    ui::LatencyInfo* latency,
    base::TimeTicks time_stamp) {
  latency->AddLatencyNumberWithTimestamp(ui::INPUT_EVENT_LATENCY_UI_COMPONENT,
                                         time_stamp);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, time_stamp);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, time_stamp);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, time_stamp);
  latency->AddLatencyNumberWithTimestamp(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, time_stamp);
}

void AddRenderingScheduledComponent(ui::LatencyInfo* latency,
                                    bool main,
                                    base::TimeTicks time_stamp) {
  if (main) {
    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, time_stamp);
  } else {
    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, time_stamp);
  }
}

}  // namespace

class RenderInputRouterLatencyTrackerTestBrowserClient
    : public TestContentBrowserClient {
 public:
  RenderInputRouterLatencyTrackerTestBrowserClient() {}

  RenderInputRouterLatencyTrackerTestBrowserClient(
      const RenderInputRouterLatencyTrackerTestBrowserClient&) = delete;
  RenderInputRouterLatencyTrackerTestBrowserClient& operator=(
      const RenderInputRouterLatencyTrackerTestBrowserClient&) = delete;

  ~RenderInputRouterLatencyTrackerTestBrowserClient() override {}

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

class RenderInputRouterLatencyTrackerTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderInputRouterLatencyTrackerTest() : old_browser_client_(nullptr) {
    ResetHistograms();
  }

  RenderInputRouterLatencyTrackerTest(
      const RenderInputRouterLatencyTrackerTest&) = delete;
  RenderInputRouterLatencyTrackerTest& operator=(
      const RenderInputRouterLatencyTrackerTest&) = delete;

  void ExpectUkmReported(ukm::SourceId source_id,
                         const char* event_name,
                         const std::vector<std::string>& metric_names,
                         size_t expected_count) {
    const ukm::TestUkmRecorder* ukm_recoder =
        test_browser_client_.GetTestUkmRecorder();

    auto entries = ukm_recoder->GetEntriesByName(event_name);
    EXPECT_EQ(expected_count, entries.size());
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      EXPECT_EQ(source_id, entry->source_id);
      for (const auto& metric_name : metric_names) {
        EXPECT_TRUE(ukm_recoder->EntryHasMetric(entry, metric_name.c_str()));
      }
    }
  }

  ::testing::AssertionResult HistogramSizeEq(const char* histogram_name,
                                             int size) {
    uint64_t histogram_size =
        histogram_tester_->GetAllSamples(histogram_name).size();
    if (static_cast<uint64_t>(size) == histogram_size) {
      return ::testing::AssertionSuccess();
    } else {
      return ::testing::AssertionFailure() << histogram_name << " expected "
                                           << size << " entries, but had "
                                           << histogram_size;
    }
  }

  RenderInputRouterLatencyTracker* tracker() { return tracker_.get(); }
  ui::LatencyTracker* viz_tracker() { return &viz_tracker_; }

  void ResetHistograms() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  const base::HistogramTester& histogram_tester() {
    return *histogram_tester_;
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);
    tracker_ = std::make_unique<RenderInputRouterLatencyTracker>(
        main_test_rfh()->GetRenderWidgetHost());
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    tracker_.reset();
    RenderViewHostImplTestHarness::TearDown();
    test_browser_client_.GetTestUkmRecorder()->Purge();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<RenderInputRouterLatencyTracker> tracker_;
  ui::LatencyTracker viz_tracker_;
  RenderInputRouterLatencyTrackerTestBrowserClient test_browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestWheelToFirstScrollHistograms \
  DISABLED_TestWheelToFirstScrollHistograms
#else
#define MAYBE_TestWheelToFirstScrollHistograms TestWheelToFirstScrollHistograms
#endif

TEST_F(RenderInputRouterLatencyTrackerTest,
       MAYBE_TestWheelToFirstScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = blink::SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStamp(now);
      ui::EventLatencyMetadata event_latency_metadata;
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          wheel, &wheel_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
      viz_tracker()->OnGpuSwapBuffersCompleted({wheel_latency});

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported(
          source_id, "Event.ScrollBegin.Wheel",
          {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
          total_ukm_entry_count);
    }
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestWheelToScrollHistograms DISABLED_TestWheelToScrollHistograms
#else
#define MAYBE_TestWheelToScrollHistograms TestWheelToScrollHistograms
#endif

TEST_F(RenderInputRouterLatencyTrackerTest, MAYBE_TestWheelToScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = blink::SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStamp(now);
      ui::EventLatencyMetadata event_latency_metadata;
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          wheel, &wheel_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
      viz_tracker()->OnGpuSwapBuffersCompleted({wheel_latency});

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported(
          source_id, "Event.ScrollUpdate.Wheel",
          {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
          total_ukm_entry_count);
    }
  }
}

TEST_F(RenderInputRouterLatencyTrackerTest,
       LatencyTerminatedOnAckIfGSUIgnored) {
  for (blink::WebGestureDevice source_device :
       {blink::WebGestureDevice::kTouchscreen,
        blink::WebGestureDevice::kTouchpad}) {
    for (bool rendering_on_main : {false, true}) {
      auto scroll = blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, source_device);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      ui::EventLatencyMetadata event_latency_metadata;
      scroll_latency.set_source_event_type(
          source_device == blink::WebGestureDevice::kTouchscreen
              ? ui::SourceEventType::TOUCH
              : ui::SourceEventType::WHEEL);
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency, &event_latency_metadata);
      tracker()->OnInputEventAck(
          scroll, &scroll_latency,
          blink::mojom::InputEventResultState::kNoConsumerExists);
      EXPECT_TRUE(scroll_latency.terminated());
    }
  }
}

TEST_F(RenderInputRouterLatencyTrackerTest, ScrollLatency) {
  auto scroll_begin = blink::SyntheticWebGestureEventBuilder::BuildScrollBegin(
      5, -5, blink::WebGestureDevice::kTouchscreen);
  ui::LatencyInfo scroll_latency;
  ui::EventLatencyMetadata event_latency_metadata;
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(scroll_begin, &scroll_latency,
                          &event_latency_metadata);
  base::TimeTicks begin_rwh_timestamp;
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
  EXPECT_EQ(2U, scroll_latency.latency_components().size());
  EXPECT_FALSE(
      event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
  EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
            begin_rwh_timestamp);

  // The first GestureScrollUpdate should be provided with
  // INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto first_scroll_update =
      blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
  scroll_latency = ui::LatencyInfo();
  event_latency_metadata = ui::EventLatencyMetadata();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(first_scroll_update, &scroll_latency,
                          &event_latency_metadata);
  begin_rwh_timestamp = base::TimeTicks();
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());
  EXPECT_FALSE(
      event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
  EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
            begin_rwh_timestamp);

  // Subsequent GestureScrollUpdates should be provided with
  // INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto scroll_update =
      blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          -5.f, 5.f, 0, blink::WebGestureDevice::kTouchscreen);
  scroll_latency = ui::LatencyInfo();
  event_latency_metadata = ui::EventLatencyMetadata();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(scroll_update, &scroll_latency,
                          &event_latency_metadata);
  begin_rwh_timestamp = base::TimeTicks();
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_EQ(3U, scroll_latency.latency_components().size());
  EXPECT_FALSE(
      event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
  EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
            begin_rwh_timestamp);
}

}  // namespace content
