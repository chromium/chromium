// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"

#include <memory>
#include <string>

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/content_client.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using blink::WebInputEvent;
using testing::ElementsAre;

namespace content {
namespace {

// Trace ids are generated in sequence in practice, but in these tests, we don't
// care about the value, so we'll just use a constant.
const int kTraceEventId = 5;
const char kUrl[] = "http://www.foo.bar.com/subpage/1";

void AddFakeComponentsWithTimeStamp(
    const RenderWidgetHostLatencyTracker& tracker,
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

void AddFakeComponents(const RenderWidgetHostLatencyTracker& tracker,
                       ui::LatencyInfo* latency) {
  base::TimeTicks now = base::TimeTicks::Now();
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
  latency->AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, now);
  AddFakeComponentsWithTimeStamp(tracker, latency, base::TimeTicks::Now());
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

class RenderWidgetHostLatencyTrackerTestBrowserClient
    : public TestContentBrowserClient {
 public:
  RenderWidgetHostLatencyTrackerTestBrowserClient() {}
  ~RenderWidgetHostLatencyTrackerTestBrowserClient() override {}

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTrackerTestBrowserClient);
};

class RenderWidgetHostLatencyTrackerTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostLatencyTrackerTest() : old_browser_client_(nullptr) {
    ResetHistograms();
  }

  void ExpectUkmReported(ukm::SourceId source_id,
                         const char* event_name,
                         const std::vector<std::string>& metric_names,
                         size_t expected_count) {
    const ukm::TestUkmRecorder* ukm_recoder =
        test_browser_client_.GetTestUkmRecorder();

    auto entries = ukm_recoder->GetEntriesByName(event_name);
    EXPECT_EQ(expected_count, entries.size());
    for (const auto* const entry : entries) {
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

  RenderWidgetHostLatencyTracker* tracker() { return tracker_.get(); }
  ui::LatencyTracker* viz_tracker() { return &viz_tracker_; }

  void ResetHistograms() {
    histogram_tester_.reset(new base::HistogramTester());
  }

  const base::HistogramTester& histogram_tester() {
    return *histogram_tester_;
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    old_browser_client_ = SetBrowserClientForTesting(&test_browser_client_);
    tracker_ = std::make_unique<RenderWidgetHostLatencyTracker>(contents());
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    RenderViewHostImplTestHarness::TearDown();
    test_browser_client_.GetTestUkmRecorder()->Purge();
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<RenderWidgetHostLatencyTracker> tracker_;
  ui::LatencyTracker viz_tracker_;
  RenderWidgetHostLatencyTrackerTestBrowserClient test_browser_client_;
  ContentBrowserClient* old_browser_client_;
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTrackerTest);
};

TEST_F(RenderWidgetHostLatencyTrackerTest, TestValidEventTiming) {
  base::TimeTicks now = base::TimeTicks::Now();

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::WHEEL);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(60));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(50));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(40));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(30));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(20));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT,
      now + base::TimeDelta::FromMilliseconds(10));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, now);

  viz_tracker()->OnGpuSwapBuffersCompleted(latency_info);

  // When last_event_time of the end_component is less than the first_event_time
  // of the start_component, zero is recorded instead of a negative value.
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin4", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.Scroll.Wheel.TimeToScrollUpdateSwapBegin2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.Scroll.Wheel.TimeToHandled2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.RendererSwapToBrowserNotified2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.BrowserNotifiedToBeforeGpuSwap2", 0, 1);
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.GpuSwap2", 0, 1);
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_TestWheelToFirstScrollHistograms \
  DISABLED_TestWheelToFirstScrollHistograms
#else
#define MAYBE_TestWheelToFirstScrollHistograms TestWheelToFirstScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest,
       MAYBE_TestWheelToFirstScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetUkmSourceIdForLastCommittedSource();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStamp(now);
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency);
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(wheel, &wheel_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      viz_tracker()->OnGpuSwapBuffersCompleted(wheel_latency);

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported(
          source_id, "Event.ScrollBegin.Wheel",
          {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
          total_ukm_entry_count);

      // UMA histograms.
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate."
                          "TimeToScrollUpdateSwapBegin2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "RendererSwapToBrowserNotified2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "BrowserNotifiedToBeforeGpuSwap2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.GpuSwap2", 1));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.RendererSwapToBrowserNotified2",
          0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.BrowserNotifiedToBeforeGpuSwap2",
          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel.GpuSwap2", 0));
    }
  }
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_TestWheelToScrollHistograms DISABLED_TestWheelToScrollHistograms
#else
#define MAYBE_TestWheelToScrollHistograms TestWheelToScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest, MAYBE_TestWheelToScrollHistograms) {
  const GURL url(kUrl);
  size_t total_ukm_entry_count = 0;
  contents()->NavigateAndCommit(url);
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetUkmSourceIdForLastCommittedSource();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
          blink::WebMouseWheelEvent::kPhaseChanged);
      base::TimeTicks now = base::TimeTicks::Now();
      wheel.SetTimeStamp(now);
      ui::LatencyInfo wheel_latency(ui::SourceEventType::WHEEL);
      wheel_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &wheel_latency, now);
      AddRenderingScheduledComponent(&wheel_latency, rendering_on_main, now);
      tracker()->OnInputEvent(wheel, &wheel_latency);
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(wheel_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(wheel, &wheel_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      viz_tracker()->OnGpuSwapBuffersCompleted(wheel_latency);

      // UKM metrics.
      total_ukm_entry_count++;
      ExpectUkmReported(
          source_id, "Event.ScrollUpdate.Wheel",
          {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
          total_ukm_entry_count);

      // UMA histograms.
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin."
                          "TimeToScrollUpdateSwapBegin2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel."
                          "TimeToScrollUpdateSwapBegin2",
                          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          1));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.TimeToHandled2_Impl", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Main", 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollBegin.Wheel.HandledToRendererSwap2_Impl", 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "RendererSwapToBrowserNotified2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel."
                          "BrowserNotifiedToBeforeGpuSwap2",
                          0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollBegin.Wheel.GpuSwap2", 0));

      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.TimeToHandled2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Main",
                          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.Scroll.Wheel.TimeToHandled2_Impl",
                          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Main",
          rendering_on_main ? 1 : 0));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.HandledToRendererSwap2_Impl",
          rendering_on_main ? 0 : 1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.RendererSwapToBrowserNotified2",
          1));
      EXPECT_TRUE(HistogramSizeEq(
          "Event.Latency.ScrollUpdate.Wheel.BrowserNotifiedToBeforeGpuSwap2",
          1));
      EXPECT_TRUE(
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel.GpuSwap2", 1));
    }
  }
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_TestInertialToScrollHistograms \
  DISABLED_TestInertialToScrollHistograms
#else
#define MAYBE_TestInertialToScrollHistograms TestInertialToScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest,
       MAYBE_TestInertialToScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency(ui::SourceEventType::INERTIAL);
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      viz_tracker()->OnGpuSwapBuffersCompleted(scroll_latency);
    }

    // UMA histograms.
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollInertial.Touch."
                        "TimeToScrollUpdateSwapBegin4",
                        1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.TimeToHandled2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.TimeToHandled2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.HandledToRendererSwap2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.HandledToRendererSwap2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.RendererSwapToBrowserNotified2",
        1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollInertial.Touch.BrowserNotifiedToBeforeGpuSwap2",
        1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollInertial.Touch.GpuSwap2", 1));
  }
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_TestTouchToFirstScrollHistograms \
  DISABLED_TestTouchToFirstScrollHistograms
#else
#define MAYBE_TestTouchToFirstScrollHistograms TestTouchToFirstScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest,
       MAYBE_TestTouchToFirstScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  size_t total_ukm_entry_count = 0;
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetUkmSourceIdForLastCommittedSource();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency);
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(touch, &touch_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      viz_tracker()->OnGpuSwapBuffersCompleted(touch_latency);
    }

    // UKM metrics.
    total_ukm_entry_count++;
    ExpectUkmReported(
        source_id, "Event.ScrollBegin.Touch",
        {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
        total_ukm_entry_count);

    // UMA histograms.
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin."
                        "TimeToScrollUpdateSwapBegin2",
                        1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate."
                        "TimeToScrollUpdateSwapBegin2",
                        0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4", 1));

    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4", 0));

    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch.TimeToHandled2_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch.TimeToHandled2_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "RendererSwapToBrowserNotified2",
                        1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "BrowserNotifiedToBeforeGpuSwap2",
                        1));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollBegin.Touch.GpuSwap2", 1));

    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.RendererSwapToBrowserNotified2", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.BrowserNotifiedToBeforeGpuSwap2", 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.GpuSwap2", 0));
  }
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_TestTouchToScrollHistograms DISABLED_TestTouchToScrollHistograms
#else
#define MAYBE_TestTouchToScrollHistograms TestTouchToScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest, MAYBE_TestTouchToScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  size_t total_ukm_entry_count = 0;
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetUkmSourceIdForLastCommittedSource();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    }

    {
      SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency);
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(touch, &touch_latency,
                                 INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      viz_tracker()->OnGpuSwapBuffersCompleted(touch_latency);
    }

    // UKM metrics.
    total_ukm_entry_count++;
    ExpectUkmReported(
        source_id, "Event.ScrollUpdate.Touch",
        {"TimeToScrollUpdateSwapBegin", "TimeToHandled", "IsMainThread"},
        total_ukm_entry_count);

    // UMA histograms.
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin."
                        "TimeToScrollUpdateSwapBegin2",
                        0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate."
                        "TimeToScrollUpdateSwapBegin2",
                        1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToHandled2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.TimeToHandled2_Impl", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Main", 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollBegin.Touch.HandledToRendererSwap2_Impl", 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "RendererSwapToBrowserNotified2",
                        0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollBegin.Touch."
                        "BrowserNotifiedToBeforeGpuSwap2",
                        0));
    EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollBegin.Touch.GpuSwap2", 0));

    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Main",
                        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.TimeToHandled2_Impl",
                        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Main",
        rendering_on_main ? 1 : 0));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.HandledToRendererSwap2_Impl",
        rendering_on_main ? 0 : 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.RendererSwapToBrowserNotified2", 1));
    EXPECT_TRUE(HistogramSizeEq(
        "Event.Latency.ScrollUpdate.Touch.BrowserNotifiedToBeforeGpuSwap2", 1));
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollUpdate.Touch.GpuSwap2", 1));
  }
}

// Flaky on Android. https://crbug.com/970841
#if defined(OS_ANDROID)
#define MAYBE_ScrollbarEndToEndHistograms DISABLED_ScrollbarEndToEndHistograms
#else
#define MAYBE_ScrollbarEndToEndHistograms ScrollbarEndToEndHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest, MAYBE_ScrollbarEndToEndHistograms) {
  // For all combinations of ScrollBegin/ScrollUpdate main/impl rendering,
  // ensure that the LatencyTracker logs the correct set of histograms.
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  ResetHistograms();
  {
    auto mouse_move =
        SyntheticWebMouseEventBuilder::Build(blink::WebMouseEvent::kMouseMove);
    base::TimeTicks now = base::TimeTicks::Now();

    const ui::LatencyComponentType scroll_components[] = {
        ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
    };
    for (ui::LatencyComponentType component : scroll_components) {
      const bool on_main[] = {true, false};
      for (bool on_main_thread : on_main) {
        ui::LatencyInfo scrollbar_latency(ui::SourceEventType::SCROLLBAR);
        AddFakeComponentsWithTimeStamp(*tracker(), &scrollbar_latency, now);
        scrollbar_latency.AddLatencyNumberWithTimestamp(component, now);
        AddRenderingScheduledComponent(&scrollbar_latency, on_main_thread, now);
        tracker()->OnInputEvent(mouse_move, &scrollbar_latency);
        EXPECT_TRUE(scrollbar_latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
        EXPECT_TRUE(scrollbar_latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
        tracker()->OnInputEventAck(mouse_move, &scrollbar_latency,
                                   INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        viz_tracker()->OnGpuSwapBuffersCompleted(scrollbar_latency);
      }
    }
  }

  const std::string scroll_types[] = {"ScrollBegin", "ScrollUpdate"};
  for (const std::string& scroll_type : scroll_types) {
    // Each histogram that doesn't take main/impl into account should have
    // two samples (one each for main and impl).
    const std::string histogram_prefix = "Event.Latency." + scroll_type;
    histogram_tester().ExpectUniqueSample(
        histogram_prefix + ".Scrollbar.TimeToScrollUpdateSwapBegin4", 0, 2);
    histogram_tester().ExpectUniqueSample(
        histogram_prefix + ".Scrollbar.RendererSwapToBrowserNotified2", 0, 2);
    histogram_tester().ExpectUniqueSample(
        histogram_prefix + ".Scrollbar.BrowserNotifiedToBeforeGpuSwap2", 0, 2);
    histogram_tester().ExpectUniqueSample(
        histogram_prefix + ".Scrollbar.GpuSwap2", 0, 2);
    const std::string main_or_impl[] = {"Main", "Impl"};
    for (const std::string& thread : main_or_impl) {
      histogram_tester().ExpectUniqueSample(
          histogram_prefix + ".Scrollbar.TimeToHandled2_" + thread, 0, 1);
      histogram_tester().ExpectUniqueSample(
          histogram_prefix + ".Scrollbar.HandledToRendererSwap2_" + thread, 0,
          1);
    }
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest,
       LatencyTerminatedOnAckIfRenderingNotScheduled) {
  {
    auto scroll = SyntheticWebGestureEventBuilder::BuildScrollBegin(
        5.f, -5.f, blink::WebGestureDevice::kTouchscreen);
    ui::LatencyInfo scroll_latency;
    AddFakeComponents(*tracker(), &scroll_latency);
    // Don't include the rendering schedule component, since we're testing the
    // case where rendering isn't scheduled.
    tracker()->OnInputEvent(scroll, &scroll_latency);
    tracker()->OnInputEventAck(scroll, &scroll_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(scroll_latency.terminated());
  }

  {
    auto wheel = SyntheticWebMouseWheelEventBuilder::Build(
        blink::WebMouseWheelEvent::kPhaseChanged);
    ui::LatencyInfo wheel_latency;
    wheel_latency.set_source_event_type(ui::SourceEventType::WHEEL);
    AddFakeComponents(*tracker(), &wheel_latency);
    tracker()->OnInputEvent(wheel, &wheel_latency);
    tracker()->OnInputEventAck(wheel, &wheel_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(wheel_latency.terminated());
  }

  {
    SyntheticWebTouchEvent touch;
    touch.PressPoint(0, 0);
    ui::LatencyInfo touch_latency;
    touch_latency.set_source_event_type(ui::SourceEventType::TOUCH);
    AddFakeComponents(*tracker(), &touch_latency);
    tracker()->OnInputEvent(touch, &touch_latency);
    tracker()->OnInputEventAck(touch, &touch_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(touch_latency.terminated());
  }

  {
    auto mouse_move =
        SyntheticWebMouseEventBuilder::Build(blink::WebMouseEvent::kMouseMove);
    ui::LatencyInfo mouse_latency;
    AddFakeComponents(*tracker(), &mouse_latency);
    tracker()->OnInputEvent(mouse_move, &mouse_latency);
    tracker()->OnInputEventAck(mouse_move, &mouse_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(mouse_latency.terminated());
  }

  {
    auto key_event =
        SyntheticWebKeyboardEventBuilder::Build(blink::WebKeyboardEvent::kChar);
    ui::LatencyInfo key_latency;
    key_latency.set_source_event_type(ui::SourceEventType::KEY_PRESS);
    AddFakeComponents(*tracker(), &key_latency);
    tracker()->OnInputEvent(key_event, &key_latency);
    tracker()->OnInputEventAck(key_event, &key_latency,
                               INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_TRUE(key_latency.terminated());
  }

  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Main", 0));
  EXPECT_TRUE(
      HistogramSizeEq("Event.Latency.ScrollUpdate.TouchToHandled_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Main", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.HandledToRendererSwap_Impl", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.RendererSwapToBrowserNotified", 0));
  EXPECT_TRUE(HistogramSizeEq(
      "Event.Latency.ScrollUpdate.BrowserNotifiedToBeforeGpuSwap", 0));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.ScrollUpdate.GpuSwap", 0));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, LatencyTerminatedOnAckIfGSUIgnored) {
  for (blink::WebGestureDevice source_device :
       {blink::WebGestureDevice::kTouchscreen,
        blink::WebGestureDevice::kTouchpad}) {
    for (bool rendering_on_main : {false, true}) {
      auto scroll = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, source_device);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      scroll_latency.set_source_event_type(
          source_device == blink::WebGestureDevice::kTouchscreen
              ? ui::SourceEventType::TOUCH
              : ui::SourceEventType::WHEEL);
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency);
      tracker()->OnInputEventAck(scroll, &scroll_latency,
                                 INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
      EXPECT_TRUE(scroll_latency.terminated());
    }
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, ScrollLatency) {
  auto scroll_begin = SyntheticWebGestureEventBuilder::BuildScrollBegin(
      5, -5, blink::WebGestureDevice::kTouchscreen);
  ui::LatencyInfo scroll_latency;
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(scroll_begin, &scroll_latency);
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_EQ(2U, scroll_latency.latency_components().size());

  // The first GestureScrollUpdate should be provided with
  // INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto first_scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(first_scroll_update, &scroll_latency);
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, nullptr));
  EXPECT_EQ(4U, scroll_latency.latency_components().size());

  // Subsequent GestureScrollUpdates should be provided with
  // INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
  auto scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      -5.f, 5.f, 0, blink::WebGestureDevice::kTouchscreen);
  scroll_latency = ui::LatencyInfo();
  scroll_latency.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  tracker()->OnInputEvent(scroll_update, &scroll_latency);
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_FALSE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, nullptr));
  EXPECT_TRUE(scroll_latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_LAST_EVENT_COMPONENT, nullptr));
  EXPECT_EQ(4U, scroll_latency.latency_components().size());
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TouchBlockingAndQueueingTime) {
  // These numbers are sensitive to where the histogram buckets are.
  int touchstart_timestamps_ms[] = {11, 25, 35};
  int touchmove_timestamps_ms[] = {1, 5, 12};
  int touchend_timestamps_ms[] = {3, 8, 12};

  for (InputEventAckState blocking :
       {INPUT_EVENT_ACK_STATE_NOT_CONSUMED, INPUT_EVENT_ACK_STATE_CONSUMED}) {
    SyntheticWebTouchEvent event;
    {
      // Touch start.
      event.PressPoint(1, 1);

      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      tracker()->OnInputEvent(event, &latency);

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[0]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[1]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[2]));

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(event.GetType(), fake_latency,
                                               blocking);

      tracker()->OnInputEventAck(event, &latency,
                                 blocking);
    }

    {
      // Touch move.
      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      event.MovePoint(0, 20, 20);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[0]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[1]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchmove_timestamps_ms[2]));

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(event.GetType(), fake_latency,
                                               blocking);
    }

    {
      // Touch end.
      ui::LatencyInfo latency;
      latency.set_source_event_type(ui::SourceEventType::TOUCH);
      event.ReleasePoint(0);
      tracker()->OnInputEvent(event, &latency);

      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_TRUE(latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));

      EXPECT_EQ(2U, latency.latency_components().size());

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::TOUCH);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[0]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[1]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(touchend_timestamps_ms[2]));

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(event.GetType(), fake_latency,
                                               blocking);
    }
  }

  // Touch start.
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[1] - touchstart_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultPrevented"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.TouchStartDefaultAllowed"),
      ElementsAre(Bucket(
          touchstart_timestamps_ms[2] - touchstart_timestamps_ms[1], 1)));

  // Touch move.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[1] - touchmove_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultPrevented"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchMoveDefaultAllowed"),
              ElementsAre(Bucket(
                  touchmove_timestamps_ms[2] - touchmove_timestamps_ms[1], 1)));

  // Touch end.
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[1] - touchend_timestamps_ms[0], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultPrevented"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.BlockingTime.TouchEndDefaultAllowed"),
              ElementsAre(Bucket(
                  touchend_timestamps_ms[2] - touchend_timestamps_ms[1], 1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyBlockingAndQueueingTime) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_ms[] = {11, 25, 35};

  for (InputEventAckState blocking :
       {INPUT_EVENT_ACK_STATE_NOT_CONSUMED, INPUT_EVENT_ACK_STATE_CONSUMED}) {
    {
      NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kRawKeyDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   base::TimeTicks::Now());
      ui::LatencyInfo latency_info;
      latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
      tracker()->OnInputEvent(event, &latency_info);

      ui::LatencyInfo fake_latency;
      fake_latency.set_trace_id(kTraceEventId);
      fake_latency.set_source_event_type(ui::SourceEventType::KEY_PRESS);
      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[0]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[1]));

      fake_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT,
          base::TimeTicks() +
              base::TimeDelta::FromMilliseconds(event_timestamps_ms[2]));

      // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
      // overwriting components.
      tracker()->ComputeInputLatencyHistograms(event.GetType(), fake_latency,
                                               blocking);

      tracker()->OnInputEventAck(event, &latency_info, blocking);
    }
  }

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.KeyPressDefaultPrevented"),
      ElementsAre(Bucket(event_timestamps_ms[1] - event_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.QueueingTime.KeyPressDefaultAllowed"),
      ElementsAre(Bucket(event_timestamps_ms[1] - event_timestamps_ms[0], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.KeyPressDefaultPrevented"),
      ElementsAre(Bucket(event_timestamps_ms[2] - event_timestamps_ms[1], 1)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Event.Latency.BlockingTime.KeyPressDefaultAllowed"),
      ElementsAre(Bucket(event_timestamps_ms[2] - event_timestamps_ms[1], 1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyEndToEndLatency) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_microseconds[] = {11, 24};

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[0]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
      base::TimeTicks() +
          base::TimeDelta::FromMicroseconds(event_timestamps_microseconds[1]));

  viz_tracker()->OnGpuSwapBuffersCompleted(latency_info);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Event.Latency.EndToEnd.KeyPress"),
      ElementsAre(Bucket(
          event_timestamps_microseconds[1] - event_timestamps_microseconds[0],
          1)));
}

// Event.Latency.(Queueing|Blocking)Time.* histograms shouldn't be reported for
// multi-finger touch.
TEST_F(RenderWidgetHostLatencyTrackerTest,
       MultiFingerTouchIgnoredForQueueingAndBlockingTimeMetrics) {
  SyntheticWebTouchEvent event;
  InputEventAckState ack_state = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;

  {
    // First touch start.
    ui::LatencyInfo latency;
    event.PressPoint(1, 1);
    tracker()->OnInputEvent(event, &latency);
    tracker()->OnInputEventAck(event, &latency, ack_state);
  }

  {
    // Additional touch start will be ignored for queueing and blocking time
    // metrics.
    int touchstart_timestamps_ms[] = {11, 25, 35};
    ui::LatencyInfo latency;
    event.PressPoint(1, 1);
    tracker()->OnInputEvent(event, &latency);

    ui::LatencyInfo fake_latency;
    fake_latency.set_trace_id(kTraceEventId);
    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[0]));

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[1]));

    fake_latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT,
        base::TimeTicks() +
            base::TimeDelta::FromMilliseconds(touchstart_timestamps_ms[2]));

    // Call ComputeInputLatencyHistograms directly to avoid OnInputEventAck
    // overwriting components.
    tracker()->ComputeInputLatencyHistograms(event.GetType(),
                                             fake_latency, ack_state);

    tracker()->OnInputEventAck(event, &latency, ack_state);
  }

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Event.Latency.QueueingTime.TouchStartDefaultAllowed"),
              ElementsAre());
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TouchpadPinchEvents) {
  ui::LatencyInfo latency;
  latency.set_trace_id(kTraceEventId);
  latency.set_source_event_type(ui::SourceEventType::TOUCHPAD);
  latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(1));
  latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(3));
  AddFakeComponentsWithTimeStamp(
      *tracker(), &latency,
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(5));
  viz_tracker()->OnGpuSwapBuffersCompleted(latency);

  EXPECT_TRUE(HistogramSizeEq("Event.Latency.EventToRender.TouchpadPinch", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.EndToEnd.TouchpadPinch", 1));
  EXPECT_TRUE(HistogramSizeEq("Event.Latency.EndToEnd.TouchpadPinch2", 1));
}

}  // namespace content
