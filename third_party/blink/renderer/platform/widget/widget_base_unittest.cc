// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "cc/trees/commit_state.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/frame_timing_details.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/content_to_visible_time_request.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"
#include "third_party/blink/renderer/platform/widget/compositing/test/stub_widget_base_client.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {
namespace {

// Counts FlushGpuChannelIfEstablished() calls.
class FlushCountingTestingPlatformSupport : public TestingPlatformSupport {
 public:
  // Platform:
  void FlushGpuChannelIfEstablished() override { ++flush_count_; }

  int flush_count() const { return flush_count_; }

 private:
  int flush_count_ = 0;
};

class TestWidgetBaseClient : public StubWidgetBaseClient {
 public:
  void set_emulated_original_screen_infos(
      std::optional<display::ScreenInfos> screen_infos) {
    emulated_original_screen_infos_ = std::move(screen_infos);
  }

  // WidgetBaseClient:
  const display::ScreenInfos& GetOriginalScreenInfos() override {
    if (emulated_original_screen_infos_) {
      return *emulated_original_screen_infos_;
    }
    if (widget_base_ && !widget_base_->screen_infos().screen_infos.empty()) {
      return widget_base_->screen_infos();
    }
    return default_screen_infos_;
  }

  void set_widget_base(WidgetBase* widget_base) { widget_base_ = widget_base; }

 private:
  raw_ptr<WidgetBase> widget_base_ = nullptr;
  display::ScreenInfos default_screen_infos_{display::ScreenInfo()};
  std::optional<display::ScreenInfos> emulated_original_screen_infos_;
};

class WidgetBaseTest : public testing::Test {
 public:
  explicit WidgetBaseTest(
      const std::vector<base::test::FeatureRef>& disabled_features = {})
      : feature_list_({features::kDelayLayerTreeViewDeletionOnLocalSwap,
                       features::kUpdateCompositorMemoryPolicyOnScreenChange},
                      disabled_features) {}

  void SetUp() override {
    mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host;
    std::ignore = widget_host.BindNewEndpointAndPassDedicatedReceiver();
    mojo::AssociatedRemote<mojom::blink::Widget> widget;
    // The widget is hidden, so it never requests a LayerTreeFrameSink.
    widget_base_ = std::make_unique<WidgetBase>(
        &client_, widget_host.Unbind(),
        widget.BindNewEndpointAndPassDedicatedReceiver(),
        scheduler::GetSingleThreadTaskRunnerForTesting(), /*hidden=*/true,
        /*never_composited=*/false, /*is_embedded=*/false,
        /*is_for_scalable_page=*/false);
    client_.set_widget_base(widget_base_.get());
    widget_base_->InitializeCompositing(
        *page_scheduler_, client_.GetOriginalScreenInfos(),
        /*settings=*/nullptr, /*frame_widget_input_handler=*/nullptr,
        /*previous_widget=*/nullptr);
  }

  void TearDown() override {
    client_.set_widget_base(nullptr);
    widget_base_.reset();
  }

  void PresentNextFrame(WidgetBase* widget, base::TimeDelta delay) {
    task_environment_.FastForwardBy(delay);
    viz::FrameTimingDetails details;
    details.presentation_feedback.timestamp = base::TimeTicks::Now();
    auto callbacks =
        std::exchange(widget->LayerTreeHost()
                          ->pending_commit_state()
                          ->pending_successful_presentation_callbacks,
                      {});
    for (auto& callback : callbacks) {
      std::move(callback).Run(details);
    }
  }

 protected:
  // Declared before `task_environment_`, so that the features are set before
  // its thread pool starts, and reset only after the thread pool has shut down.
  // Thread pool threads also check features outside of tasks (e.g. on Android,
  // when they start), so changing features while they run is racy.
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<FlushCountingTestingPlatformSupport> platform_;
  TestWidgetBaseClient client_;
  std::unique_ptr<PageScheduler> page_scheduler_ =
      scheduler::CreateDummyPageScheduler();
  std::unique_ptr<WidgetBase> widget_base_;
};

// Releasing a LayerTreeView destroys its SharedImages. The destruction requests
// are deferred GPU channel messages, so the channel needs to be flushed right
// after the (delayed) release.
TEST_F(WidgetBaseTest, ReleasingLayerTreeViewFlushesGpuChannel) {
  const base::TimeDelta delay =
      features::kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam.Get();
  widget_base_->Shutdown(/*delay_release=*/true);
  client_.set_widget_base(nullptr);
  widget_base_.reset();

  task_environment_.FastForwardBy(delay - base::Milliseconds(1));
  EXPECT_EQ(platform_->flush_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(platform_->flush_count(), 1);
}

// WidgetBaseTest with the flush disabled by its kill switch.
class WidgetBaseKillSwitchTest : public WidgetBaseTest {
 public:
  WidgetBaseKillSwitchTest()
      : WidgetBaseTest({features::kFlushGpuChannelOnLayerTreeViewRelease}) {}
};

TEST_F(WidgetBaseKillSwitchTest, ReleasingLayerTreeViewDoesNotFlushGpuChannel) {
  widget_base_->Shutdown(/*delay_release=*/true);
  client_.set_widget_base(nullptr);
  widget_base_.reset();

  task_environment_.FastForwardBy(
      features::kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam.Get());
  EXPECT_EQ(platform_->flush_count(), 0);
}

TEST_F(WidgetBaseTest,
       RecordsSwitchPhaseDurationsOnlyOnPresentedNoSavedFramesLoadedSwitch) {
  constexpr char kBrowserMainThreadHistogram[] =
      "Browser.Tabs.NoSavedFramesLoaded.SwitchPhaseDuration.BrowserMainThread";
  constexpr char kBrowserToRendererMainThreadHistogram[] =
      "Browser.Tabs.NoSavedFramesLoaded.SwitchPhaseDuration."
      "BrowserToRendererMainThread";
  constexpr char kRendererMainThreadToPresentationHistogram[] =
      "Browser.Tabs.NoSavedFramesLoaded.SwitchPhaseDuration."
      "RendererMainThreadToPresentation";
  constexpr char kTotalDurationHistogram[] =
      "Browser.Tabs.TotalSwitchDuration3.NoSavedFrames_Loaded";
  const auto expect_phase_durations =
      [&](const base::HistogramTester& histogram_tester,
          base::TimeDelta browser_main_thread,
          base::TimeDelta browser_to_renderer_main_thread,
          base::TimeDelta renderer_main_thread_to_presentation) {
        histogram_tester.ExpectUniqueTimeSample(kBrowserMainThreadHistogram,
                                                browser_main_thread, 1);
        histogram_tester.ExpectUniqueTimeSample(
            kBrowserToRendererMainThreadHistogram,
            browser_to_renderer_main_thread, 1);
        histogram_tester.ExpectUniqueTimeSample(
            kRendererMainThreadToPresentationHistogram,
            renderer_main_thread_to_presentation, 1);
      };
  const auto expect_no_phase_durations =
      [&](const base::HistogramTester& histogram_tester) {
        histogram_tester.ExpectTotalCount(kBrowserMainThreadHistogram, 0);
        histogram_tester.ExpectTotalCount(kBrowserToRendererMainThreadHistogram,
                                          0);
        histogram_tester.ExpectTotalCount(
            kRendererMainThreadToPresentationHistogram, 0);
      };

  // 1. Evicted loaded tab switch emits upon presentation (not eagerly in
  // `WasShown()`), matching `TotalSwitchDuration3.NoSavedFrames_Loaded`.
  {
    base::HistogramTester histogram_tester;
    const base::TimeTicks start_time = base::TimeTicks::Now();
    task_environment_.FastForwardBy(base::Milliseconds(345));

    RecordContentToVisibleTimeRequest request;
    request.events.push_back(VisibleTimeEvent{
        start_time,
        VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/true,
                                          /*had_saved_frame_at_start=*/false,
                                          /*destination_is_frozen=*/false}});
    request.browser_dispatch_time = start_time + base::Milliseconds(300);
    widget_base_->WasShown(/*was_evicted=*/true, request);
    expect_no_phase_durations(histogram_tester);

    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    expect_phase_durations(histogram_tester, base::Milliseconds(300),
                           base::Milliseconds(45), base::Milliseconds(16));
    histogram_tester.ExpectUniqueTimeSample(kTotalDurationHistogram,
                                            base::Milliseconds(361), 1);
  }

  // 2. Already-painting widget (e.g. `kHiddenButPainting` while captured)
  // routed via `RequestSuccessfulPresentationTimeForNextFrame()` also records
  // phase durations alongside `TotalSwitchDuration3.NoSavedFrames_Loaded`.
  {
    base::HistogramTester histogram_tester;
    const base::TimeTicks start_time = base::TimeTicks::Now();
    task_environment_.FastForwardBy(base::Milliseconds(210));

    RecordContentToVisibleTimeRequest request;
    request.events.push_back(VisibleTimeEvent{
        start_time,
        VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/true,
                                          /*had_saved_frame_at_start=*/false,
                                          /*destination_is_frozen=*/false}});
    request.browser_dispatch_time = start_time + base::Milliseconds(200);
    widget_base_->RequestSuccessfulPresentationTimeForNextFrame(request);
    expect_no_phase_durations(histogram_tester);

    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    expect_phase_durations(histogram_tester, base::Milliseconds(200),
                           base::Milliseconds(10), base::Milliseconds(16));
    histogram_tester.ExpectUniqueTimeSample(kTotalDurationHistogram,
                                            base::Milliseconds(226), 1);
    widget_base_->WasHidden();
  }

  // 3. Clock skew (browser timestamps in the future of the renderer clock)
  // clamps to 0ms, and a combined BFCache + NoSavedFrames_Loaded request
  // records 1 sample per phase upon presentation.
  {
    base::HistogramTester histogram_tester;
    RecordContentToVisibleTimeRequest combined_request;
    combined_request.events.push_back(VisibleTimeEvent{
        base::TimeTicks::Now() + base::Milliseconds(20),
        VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/true,
                                          /*had_saved_frame_at_start=*/false,
                                          /*destination_is_frozen=*/true}});
    combined_request.events.push_back(
        VisibleTimeEvent{base::TimeTicks::Now() - base::Milliseconds(50),
                         VisibleTimeEvent::BFCacheRestoreReason{}});
    combined_request.browser_dispatch_time =
        base::TimeTicks::Now() + base::Milliseconds(30);
    widget_base_->WasShown(/*was_evicted=*/true, combined_request);
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    expect_phase_durations(histogram_tester, base::Milliseconds(10),
                           base::TimeDelta(), base::Milliseconds(16));
    widget_base_->WasHidden();
  }

  // 4. Non-presented switches (hidden before presentation, cancelled while
  // captured, missed hide before presentation, or widget shutdown before
  // presentation) emit zero samples to both histograms.
  {
    base::HistogramTester histogram_tester;
    const auto make_evicted_loaded_request = []() {
      RecordContentToVisibleTimeRequest request;
      request.events.push_back(VisibleTimeEvent{
          base::TimeTicks::Now() - base::Milliseconds(1200),
          VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/true,
                                            /*had_saved_frame_at_start=*/false,
                                            /*destination_is_frozen=*/false}});
      request.browser_dispatch_time =
          base::TimeTicks::Now() - base::Milliseconds(1000);
      return request;
    };

    // Aborted via `WasHidden()` before presentation.
    widget_base_->WasShown(/*was_evicted=*/true, make_evicted_loaded_request());
    widget_base_->WasHidden();
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));

    // Cancelled via `CancelSuccessfulPresentationTimeRequest()` while painting.
    widget_base_->WasShown(/*was_evicted=*/false, std::nullopt);
    widget_base_->RequestSuccessfulPresentationTimeForNextFrame(
        make_evicted_loaded_request());
    widget_base_->CancelSuccessfulPresentationTimeRequest();
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    widget_base_->WasHidden();

    // Missed hide (second `WasShown()` overwrites first before presentation),
    // then hidden before presentation.
    widget_base_->WasShown(/*was_evicted=*/true, make_evicted_loaded_request());
    widget_base_->WasShown(/*was_evicted=*/true, make_evicted_loaded_request());
    widget_base_->WasHidden();
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));

    // Widget destroyed before presentation.
    mojo::AssociatedRemote<mojom::blink::WidgetHost> transient_host;
    std::ignore = transient_host.BindNewEndpointAndPassDedicatedReceiver();
    mojo::AssociatedRemote<mojom::blink::Widget> transient_remote;
    auto transient_widget = std::make_unique<WidgetBase>(
        &client_, transient_host.Unbind(),
        transient_remote.BindNewEndpointAndPassDedicatedReceiver(),
        scheduler::GetSingleThreadTaskRunnerForTesting(), /*hidden=*/true,
        /*never_composited=*/false, /*is_embedded=*/false,
        /*is_for_scalable_page=*/false);
    transient_widget->InitializeCompositing(
        *page_scheduler_, client_.GetOriginalScreenInfos(),
        /*settings=*/nullptr, /*frame_widget_input_handler=*/nullptr,
        /*previous_widget=*/nullptr);
    transient_widget->WasShown(/*was_evicted=*/true,
                               make_evicted_loaded_request());
    auto orphan_callbacks =
        std::exchange(transient_widget->LayerTreeHost()
                          ->pending_commit_state()
                          ->pending_successful_presentation_callbacks,
                      {});
    transient_widget->Shutdown(/*delay_release=*/false);
    transient_widget.reset();
    viz::FrameTimingDetails details;
    details.presentation_feedback.timestamp = base::TimeTicks::Now();
    for (auto& callback : orphan_callbacks) {
      std::move(callback).Run(details);
    }

    expect_no_phase_durations(histogram_tester);
    histogram_tester.ExpectTotalCount(kTotalDurationHistogram, 0);
  }

  // 5. Unloaded evicted tab switch, saved-frame tab switch (combined with
  // BFCache), pure BFCache restore, and nullopt request emit zero samples even
  // when presented.
  {
    base::HistogramTester histogram_tester;
    RecordContentToVisibleTimeRequest not_loaded_request;
    not_loaded_request.events.push_back(VisibleTimeEvent{
        base::TimeTicks::Now() - base::Milliseconds(120),
        VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/false,
                                          /*had_saved_frame_at_start=*/false,
                                          /*destination_is_frozen=*/false}});
    not_loaded_request.browser_dispatch_time = base::TimeTicks::Now();
    widget_base_->WasShown(/*was_evicted=*/true, not_loaded_request);
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    widget_base_->WasHidden();

    RecordContentToVisibleTimeRequest saved_frame_and_bfcache_request;
    saved_frame_and_bfcache_request.events.push_back(VisibleTimeEvent{
        base::TimeTicks::Now() - base::Milliseconds(90),
        VisibleTimeEvent::TabSwitchReason{/*destination_is_loaded=*/true,
                                          /*had_saved_frame_at_start=*/true,
                                          /*destination_is_frozen=*/false}});
    saved_frame_and_bfcache_request.events.push_back(
        VisibleTimeEvent{base::TimeTicks::Now() - base::Milliseconds(90),
                         VisibleTimeEvent::BFCacheRestoreReason{}});
    saved_frame_and_bfcache_request.browser_dispatch_time =
        base::TimeTicks::Now();
    widget_base_->WasShown(/*was_evicted=*/false,
                           saved_frame_and_bfcache_request);
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    widget_base_->WasHidden();

    RecordContentToVisibleTimeRequest bfcache_request;
    bfcache_request.events.push_back(
        VisibleTimeEvent{base::TimeTicks::Now() - base::Milliseconds(80),
                         VisibleTimeEvent::BFCacheRestoreReason{}});
    bfcache_request.browser_dispatch_time = base::TimeTicks::Now();
    widget_base_->WasShown(/*was_evicted=*/false, bfcache_request);
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    widget_base_->WasHidden();

    widget_base_->WasShown(/*was_evicted=*/false, std::nullopt);
    PresentNextFrame(widget_base_.get(), base::Milliseconds(16));
    widget_base_->WasHidden();

    expect_no_phase_durations(histogram_tester);
  }

  widget_base_->Shutdown(/*delay_release=*/false);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(WidgetBaseTest, UpdateScreenInfoUpdatesLayerTreeHostMemoryPolicy) {
  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos external_screen_infos(external_screen_info);
  widget_base_->UpdateScreenInfo(external_screen_infos);

  const cc::ManagedMemoryPolicy expected_external_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         external_screen_info.rect.size(),
                         external_screen_info.device_scale_factor);
  EXPECT_EQ(expected_external_policy,
            widget_base_->LayerTreeHost()->memory_policy());

  // Emulating a smaller mobile screen while GetOriginalScreenInfos() remains on
  // the external display should keep the physical display's memory policy.
  client_.set_emulated_original_screen_infos(external_screen_infos);
  display::ScreenInfo emulated_mobile_screen_info;
  emulated_mobile_screen_info.rect = gfx::Rect(0, 0, 375, 667);
  emulated_mobile_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(
      display::ScreenInfos(emulated_mobile_screen_info));
  EXPECT_EQ(expected_external_policy,
            widget_base_->LayerTreeHost()->memory_policy());
  client_.set_emulated_original_screen_infos(std::nullopt);

  display::ScreenInfo internal_screen_info;
  internal_screen_info.rect = gfx::Rect(0, 0, 1440, 900);
  internal_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos internal_screen_infos(internal_screen_info);
  widget_base_->UpdateScreenInfo(internal_screen_infos);

  const cc::ManagedMemoryPolicy expected_internal_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         internal_screen_info.rect.size(),
                         internal_screen_info.device_scale_factor);
  EXPECT_EQ(expected_internal_policy,
            widget_base_->LayerTreeHost()->memory_policy());
  EXPECT_LT(expected_internal_policy.bytes_limit_when_visible,
            expected_external_policy.bytes_limit_when_visible);

  widget_base_->Shutdown(/*delay_release=*/false);
}

TEST_F(WidgetBaseTest,
       InitializeCompositingWithPreviousWidgetUpdatesMemoryPolicy) {
  display::ScreenInfo internal_screen_info;
  internal_screen_info.rect = gfx::Rect(0, 0, 1440, 900);
  internal_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(display::ScreenInfos(internal_screen_info));

  const cc::ManagedMemoryPolicy expected_internal_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         internal_screen_info.rect.size(),
                         internal_screen_info.device_scale_factor);
  EXPECT_EQ(expected_internal_policy,
            widget_base_->LayerTreeHost()->memory_policy());

  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos external_screen_infos(external_screen_info);

  mojo::AssociatedRemote<mojom::blink::WidgetHost> next_widget_host;
  std::ignore = next_widget_host.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<mojom::blink::Widget> next_widget_remote;
  auto next_widget = std::make_unique<WidgetBase>(
      &client_, next_widget_host.Unbind(),
      next_widget_remote.BindNewEndpointAndPassDedicatedReceiver(),
      scheduler::GetSingleThreadTaskRunnerForTesting(), /*hidden=*/true,
      /*never_composited=*/false, /*is_embedded=*/false,
      /*is_for_scalable_page=*/false);
  client_.set_widget_base(next_widget.get());
  next_widget->InitializeCompositing(*page_scheduler_, external_screen_infos,
                                     /*settings=*/nullptr,
                                     /*frame_widget_input_handler=*/nullptr,
                                     /*previous_widget=*/widget_base_.get());

  const cc::ManagedMemoryPolicy expected_external_policy =
      GetGpuMemoryPolicy(next_widget->LayerTreeHost()->memory_policy(),
                         external_screen_info.rect.size(),
                         external_screen_info.device_scale_factor);
  EXPECT_EQ(expected_external_policy,
            next_widget->LayerTreeHost()->memory_policy());

  client_.set_widget_base(nullptr);
  widget_base_->Shutdown(/*delay_release=*/false);
  next_widget->Shutdown(/*delay_release=*/false);
}

class WidgetBaseMemoryPolicyKillSwitchTest : public WidgetBaseTest {
 public:
  WidgetBaseMemoryPolicyKillSwitchTest()
      : WidgetBaseTest(
            {features::kUpdateCompositorMemoryPolicyOnScreenChange}) {}
};

TEST_F(WidgetBaseMemoryPolicyKillSwitchTest,
       UpdateScreenInfoDoesNotUpdateLayerTreeHostMemoryPolicyWhenDisabled) {
  const cc::ManagedMemoryPolicy initial_policy =
      widget_base_->LayerTreeHost()->memory_policy();

  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(display::ScreenInfos(external_screen_info));

  EXPECT_EQ(initial_policy, widget_base_->LayerTreeHost()->memory_policy());

  widget_base_->Shutdown(/*delay_release=*/false);
}
#endif

}  // namespace
}  // namespace blink
