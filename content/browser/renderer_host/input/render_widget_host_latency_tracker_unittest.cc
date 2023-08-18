// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/input/native_web_keyboard_event.h"
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

  RenderWidgetHostLatencyTrackerTestBrowserClient(
      const RenderWidgetHostLatencyTrackerTestBrowserClient&) = delete;
  RenderWidgetHostLatencyTrackerTestBrowserClient& operator=(
      const RenderWidgetHostLatencyTrackerTestBrowserClient&) = delete;

  ~RenderWidgetHostLatencyTrackerTestBrowserClient() override {}

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

class RenderWidgetHostLatencyTrackerTest
    : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostLatencyTrackerTest() : old_browser_client_(nullptr) {
    ResetHistograms();
  }

  RenderWidgetHostLatencyTrackerTest(
      const RenderWidgetHostLatencyTrackerTest&) = delete;
  RenderWidgetHostLatencyTrackerTest& operator=(
      const RenderWidgetHostLatencyTrackerTest&) = delete;

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
    histogram_tester_ = std::make_unique<base::HistogramTester>();
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
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

TEST_F(RenderWidgetHostLatencyTrackerTest, TestValidEventTiming) {
  base::TimeTicks now = base::TimeTicks::Now();

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::WHEEL);

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
      now + base::Milliseconds(60));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
      now + base::Milliseconds(50));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
      now + base::Milliseconds(40));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT,
      now + base::Milliseconds(30));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      now + base::Milliseconds(20));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, now + base::Milliseconds(10));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, now);

  viz_tracker()->OnGpuSwapBuffersCompleted({latency_info});

  // When last_event_time of the end_component is less than the first_event_time
  // of the start_component, zero is recorded instead of a negative value.
  histogram_tester().ExpectUniqueSample(
      "Event.Latency.ScrollBegin.Wheel.TimeToScrollUpdateSwapBegin4", 0, 1);
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
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
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          0));
    }
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestWheelToScrollHistograms DISABLED_TestWheelToScrollHistograms
#else
#define MAYBE_TestWheelToScrollHistograms TestWheelToScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest, MAYBE_TestWheelToScrollHistograms) {
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
          HistogramSizeEq("Event.Latency.ScrollUpdate.Wheel."
                          "TimeToScrollUpdateSwapBegin4",
                          1));
    }
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
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
      auto scroll = blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency(ui::SourceEventType::INERTIAL);
      ui::EventLatencyMetadata event_latency_metadata;
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          scroll, &scroll_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
      viz_tracker()->OnGpuSwapBuffersCompleted({scroll_latency});
    }

    // UMA histograms.
    EXPECT_TRUE(
        HistogramSizeEq("Event.Latency.ScrollInertial.Touch."
                        "TimeToScrollUpdateSwapBegin4",
                        1));
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
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
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      ui::EventLatencyMetadata event_latency_metadata;
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          scroll, &scroll_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
    }

    {
      blink::SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::EventLatencyMetadata event_latency_metadata;
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          touch, &touch_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
      viz_tracker()->OnGpuSwapBuffersCompleted({touch_latency});
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
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_TestTouchToScrollHistograms DISABLED_TestTouchToScrollHistograms
#else
#define MAYBE_TestTouchToScrollHistograms TestTouchToScrollHistograms
#endif

TEST_F(RenderWidgetHostLatencyTrackerTest, MAYBE_TestTouchToScrollHistograms) {
  const GURL url(kUrl);
  contents()->NavigateAndCommit(url);
  size_t total_ukm_entry_count = 0;
  ukm::SourceId source_id = static_cast<WebContentsImpl*>(contents())
                                ->GetPrimaryMainFrame()
                                ->GetPageUkmSourceId();
  EXPECT_NE(ukm::kInvalidSourceId, source_id);
  for (bool rendering_on_main : {false, true}) {
    ResetHistograms();
    {
      auto scroll = blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
          5.f, -5.f, 0, blink::WebGestureDevice::kTouchscreen);
      base::TimeTicks now = base::TimeTicks::Now();
      scroll.SetTimeStamp(now);
      ui::LatencyInfo scroll_latency;
      ui::EventLatencyMetadata event_latency_metadata;
      AddFakeComponentsWithTimeStamp(*tracker(), &scroll_latency, now);
      AddRenderingScheduledComponent(&scroll_latency, rendering_on_main, now);
      tracker()->OnInputEvent(scroll, &scroll_latency, &event_latency_metadata);
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
      EXPECT_TRUE(scroll_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      tracker()->OnInputEventAck(
          scroll, &scroll_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
    }

    {
      blink::SyntheticWebTouchEvent touch;
      touch.PressPoint(0, 0);
      touch.PressPoint(1, 1);
      ui::LatencyInfo touch_latency(ui::SourceEventType::TOUCH);
      ui::EventLatencyMetadata event_latency_metadata;
      base::TimeTicks now = base::TimeTicks::Now();
      touch_latency.AddLatencyNumberWithTimestamp(
          ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT, now);
      AddFakeComponentsWithTimeStamp(*tracker(), &touch_latency, now);
      AddRenderingScheduledComponent(&touch_latency, rendering_on_main, now);
      tracker()->OnInputEvent(touch, &touch_latency, &event_latency_metadata);
      base::TimeTicks begin_rwh_timestamp;
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
      EXPECT_TRUE(touch_latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
      EXPECT_FALSE(
          event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
      EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                begin_rwh_timestamp);
      tracker()->OnInputEventAck(
          touch, &touch_latency,
          blink::mojom::InputEventResultState::kNotConsumed);
      viz_tracker()->OnGpuSwapBuffersCompleted({touch_latency});
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
  }
}

// Flaky on Android. https://crbug.com/970841
#if BUILDFLAG(IS_ANDROID)
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
    auto mouse_move = blink::SyntheticWebMouseEventBuilder::Build(
        blink::WebMouseEvent::Type::kMouseMove);
    base::TimeTicks now = base::TimeTicks::Now();

    const ui::LatencyComponentType scroll_components[] = {
        ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
        ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
    };
    for (ui::LatencyComponentType component : scroll_components) {
      const bool on_main[] = {true, false};
      for (bool on_main_thread : on_main) {
        ui::LatencyInfo scrollbar_latency(ui::SourceEventType::SCROLLBAR);
        ui::EventLatencyMetadata event_latency_metadata;
        AddFakeComponentsWithTimeStamp(*tracker(), &scrollbar_latency, now);
        scrollbar_latency.AddLatencyNumberWithTimestamp(component, now);
        AddRenderingScheduledComponent(&scrollbar_latency, on_main_thread, now);
        tracker()->OnInputEvent(mouse_move, &scrollbar_latency,
                                &event_latency_metadata);
        base::TimeTicks begin_rwh_timestamp;
        EXPECT_TRUE(scrollbar_latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, &begin_rwh_timestamp));
        EXPECT_TRUE(scrollbar_latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, nullptr));
        EXPECT_FALSE(
            event_latency_metadata.arrived_in_browser_main_timestamp.is_null());
        EXPECT_EQ(event_latency_metadata.arrived_in_browser_main_timestamp,
                  begin_rwh_timestamp);
        tracker()->OnInputEventAck(
            mouse_move, &scrollbar_latency,
            blink::mojom::InputEventResultState::kNotConsumed);
        viz_tracker()->OnGpuSwapBuffersCompleted({scrollbar_latency});
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
  }
}

TEST_F(RenderWidgetHostLatencyTrackerTest, LatencyTerminatedOnAckIfGSUIgnored) {
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

TEST_F(RenderWidgetHostLatencyTrackerTest, ScrollLatency) {
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

TEST_F(RenderWidgetHostLatencyTrackerTest, KeyEndToEndLatency) {
  // These numbers are sensitive to where the histogram buckets are.
  int event_timestamps_microseconds[] = {11, 24};

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(kTraceEventId);
  latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      base::TimeTicks() + base::Microseconds(event_timestamps_microseconds[0]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      base::TimeTicks() + base::Microseconds(event_timestamps_microseconds[0]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT,
      base::TimeTicks() + base::Microseconds(event_timestamps_microseconds[1]));

  latency_info.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
      base::TimeTicks() + base::Microseconds(event_timestamps_microseconds[1]));

  viz_tracker()->OnGpuSwapBuffersCompleted({latency_info});

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Event.Latency.EndToEnd.KeyPress"),
      ElementsAre(Bucket(
          event_timestamps_microseconds[1] - event_timestamps_microseconds[0],
          1)));
}

TEST_F(RenderWidgetHostLatencyTrackerTest, TouchpadPinchEvents) {
  ui::LatencyInfo latency;
  latency.set_trace_id(kTraceEventId);
  latency.set_source_event_type(ui::SourceEventType::TOUCHPAD);
  latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
      base::TimeTicks() + base::Milliseconds(1));
  latency.AddLatencyNumberWithTimestamp(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      base::TimeTicks() + base::Milliseconds(3));
  AddFakeComponentsWithTimeStamp(*tracker(), &latency,
                                 base::TimeTicks() + base::Milliseconds(5));
  viz_tracker()->OnGpuSwapBuffersCompleted({latency});

  EXPECT_TRUE(HistogramSizeEq("Event.Latency.EndToEnd.TouchpadPinch2", 1));
}

}  // namespace content
